var Clay = require('@rebble/clay');
var clayConfig = require('./config');
var clay = new Clay(clayConfig);

// Emulator only. In the emulator Clay proxies the configuration page through a
// remote site that is served over HTTP, which browsers that prefer HTTPS stall
// on for minutes. The same proxy page is kept in dev/clay-proxy.html and is
// served from disk instead. The path below is absolute and has to be adjusted
// if the repository moves. On a phone this branch never runs, Clay opening the
// configuration page directly.
if (typeof Pebble === 'undefined' || Pebble.platform === 'pypkjs') {
  var emuGenerateUrl = clay.generateUrl.bind(clay);
  clay.generateUrl = function() {
    return emuGenerateUrl().replace(
      'http://clay.pebble.com.s3-website-us-west-2.amazonaws.com/#',
      'file:///Users/cole/4K-weeks/dev/clay-proxy.html#'
    );
  };
}

/**
 * "Estimate from my location" for the weeks-left pill.
 *
 * Runs entirely in PebbleKit JS, not in the (sandboxed) Clay config page.
 * Only fires when the USE_LOCATION toggle is on. Asks the phone for a GPS
 * fix, then sends just the two coordinates to BigDataCloud's free reverse-
 * geocoding endpoint (no API key, no account, no other data) to resolve a
 * country code, and forwards that to the watch as the same COUNTRY message
 * key a manual pick would send. Any failure — no permission, no signal, no
 * network, an unrecognized country — is silent: the watch keeps whatever
 * country was last set (manually or otherwise).
 *
 * On success the resolved code is also mirrored into localStorage as
 * 'loc_country', which is what lets the webviewclosed handler below tell a
 * genuine manual override of the country field apart from an unrelated
 * settings save that just resubmits the same value Clay pre-filled. It is
 * also written into Clay's own settings cache (clay.setSettings()), which is
 * what the config page reads to pre-fill itself — without this, the Country
 * field would keep showing whatever was last manually saved instead of the
 * value location estimation actually put on the watch, the next time
 * settings are opened.
 */
function estimateCountryFromLocation() {
  if (typeof navigator === 'undefined' || !navigator.geolocation) {
    return;
  }
  navigator.geolocation.getCurrentPosition(
    function(pos) {
      var url = 'https://api.bigdatacloud.net/data/reverse-geocode-client'
          + '?latitude=' + pos.coords.latitude
          + '&longitude=' + pos.coords.longitude
          + '&localityLanguage=en';
      fetch(url)
        .then(function(res) { return res.json(); })
        .then(function(data) {
          if (data && data.countryCode) {
            Pebble.sendAppMessage({ 'COUNTRY': data.countryCode });
            try { localStorage.setItem('loc_country', data.countryCode); } catch (err) { /* private/incognito webview */ }
            clay.setSettings('COUNTRY', data.countryCode);
          }
        })
        .catch(function() { /* offline, blocked, or an unexpected reply: keep the current country */ });
    },
    function() { /* permission denied or no fix: keep the current country */ },
    { timeout: 15000, maximumAge: 21600000 }  // reuse a fix up to 6h old
  );
}

// Mirrors the USE_LOCATION toggle into localStorage (private to this watch's
// PebbleKit JS) so trigger (b) below can re-check it on every launch without
// needing to ask the watch, which does not talk back to pkjs on its own.
Pebble.addEventListener('webviewclosed', function(e) {
  if (!e || !e.response) {
    return;  // settings page was cancelled, not saved
  }
  var settings = clay.getSettings(e.response);
  var useLocation = !!settings['USE_LOCATION'];
  var country = settings['COUNTRY'];

  // Clay already sent the settings as submitted — USE_LOCATION on and a
  // manually picked COUNTRY both take effect on the watch immediately.
  // Left alone, useLocation being true below would still re-run the GPS
  // lookup right after this save (and again on every future launch),
  // silently overwriting the country just chosen. Treat a country that
  // differs from the one location estimation itself last set as a
  // deliberate override: turn location back off and correct the watch.
  if (useLocation && country) {
    var lastLocCountry = null;
    try { lastLocCountry = localStorage.getItem('loc_country'); } catch (err) { /* private/incognito webview */ }
    if (lastLocCountry && country !== lastLocCountry) {
      useLocation = false;
      Pebble.sendAppMessage({ 'USE_LOCATION': 0 });  // overrides what Clay just sent
      clay.setSettings('USE_LOCATION', false);  // so the toggle shows off next time settings open, too
    }
  }

  try {
    localStorage.setItem('use_location', useLocation ? 'true' : 'false');
  } catch (err) { /* private/incognito webview: just skip the location refresh */ }
  if (useLocation) {
    estimateCountryFromLocation();
  }
});

Pebble.addEventListener('ready', function() {
  try {
    if (localStorage.getItem('use_location') === 'true') {
      estimateCountryFromLocation();
    }
  } catch (err) { /* private/incognito webview: just skip the location refresh */ }
});
