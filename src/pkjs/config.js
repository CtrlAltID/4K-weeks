// Country options mirror src/c/life_expectancy.h's table 1:1 (same codes, same
// order) so a manual pick and a location-derived COUNTRY resolve to the same
// life expectancy on the watch.
var COUNTRY_OPTIONS = [
  { "label": "World average", "value": "WORLD" },
  { "label": "United States", "value": "US" },
  { "label": "United Kingdom", "value": "GB" },
  { "label": "Canada", "value": "CA" },
  { "label": "Australia", "value": "AU" },
  { "label": "New Zealand", "value": "NZ" },
  { "label": "Ireland", "value": "IE" },
  { "label": "Germany", "value": "DE" },
  { "label": "France", "value": "FR" },
  { "label": "Italy", "value": "IT" },
  { "label": "Spain", "value": "ES" },
  { "label": "Portugal", "value": "PT" },
  { "label": "Netherlands", "value": "NL" },
  { "label": "Belgium", "value": "BE" },
  { "label": "Switzerland", "value": "CH" },
  { "label": "Austria", "value": "AT" },
  { "label": "Sweden", "value": "SE" },
  { "label": "Norway", "value": "NO" },
  { "label": "Denmark", "value": "DK" },
  { "label": "Finland", "value": "FI" },
  { "label": "Iceland", "value": "IS" },
  { "label": "Poland", "value": "PL" },
  { "label": "Czechia", "value": "CZ" },
  { "label": "Slovakia", "value": "SK" },
  { "label": "Hungary", "value": "HU" },
  { "label": "Romania", "value": "RO" },
  { "label": "Bulgaria", "value": "BG" },
  { "label": "Greece", "value": "GR" },
  { "label": "Croatia", "value": "HR" },
  { "label": "Slovenia", "value": "SI" },
  { "label": "Russia", "value": "RU" },
  { "label": "Ukraine", "value": "UA" },
  { "label": "Turkey", "value": "TR" },
  { "label": "Israel", "value": "IL" },
  { "label": "Saudi Arabia", "value": "SA" },
  { "label": "United Arab Emirates", "value": "AE" },
  { "label": "China", "value": "CN" },
  { "label": "Japan", "value": "JP" },
  { "label": "South Korea", "value": "KR" },
  { "label": "India", "value": "IN" },
  { "label": "Pakistan", "value": "PK" },
  { "label": "Bangladesh", "value": "BD" },
  { "label": "Indonesia", "value": "ID" },
  { "label": "Thailand", "value": "TH" },
  { "label": "Vietnam", "value": "VN" },
  { "label": "Philippines", "value": "PH" },
  { "label": "Malaysia", "value": "MY" },
  { "label": "Singapore", "value": "SG" },
  { "label": "Brazil", "value": "BR" },
  { "label": "Mexico", "value": "MX" },
  { "label": "Argentina", "value": "AR" },
  { "label": "Chile", "value": "CL" },
  { "label": "Colombia", "value": "CO" },
  { "label": "Peru", "value": "PE" },
  { "label": "South Africa", "value": "ZA" },
  { "label": "Nigeria", "value": "NG" },
  { "label": "Egypt", "value": "EG" },
  { "label": "Kenya", "value": "KE" },
  { "label": "Ethiopia", "value": "ET" },
  { "label": "Ghana", "value": "GH" }
];

module.exports = [
  {
    "type": "heading",
    "defaultValue": "4K-weeks"
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Dial"
      },
      {
        "type": "select",
        "messageKey": "SCALE_STYLE",
        "label": "Scale",
        "defaultValue": 2,
        "options": [
          { "label": "Quarters", "value": 2 },
          { "label": "Tenths", "value": 1 },
          { "label": "Classic", "value": 0 }
        ]
      },
      {
        "type": "select",
        "messageKey": "HOUR_SIZE",
        "label": "Hour numbers",
        "defaultValue": 0,
        "options": [
          { "label": "Standard", "value": 0 },
          { "label": "Large", "value": 1 },
          { "label": "Larger (no minute circle)", "value": 2 },
          { "label": "Largest (no minute circle)", "value": 3 }
        ]
      }
    ]
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Hand"
      },
      {
        "type": "select",
        "messageKey": "HAND_STYLE",
        "label": "Length",
        "defaultValue": 0,
        "options": [
          { "label": "Needle", "value": 0 },
          { "label": "Short needle", "value": 2 },
          { "label": "Bar across the dial", "value": 1 }
        ]
      },
      {
        "type": "toggle",
        "messageKey": "SHOW_MINUTE",
        "label": "Minute circle",
        "defaultValue": true
      },
      {
        "type": "toggle",
        "messageKey": "MINUTE_COLON",
        "label": "Colon before the minute",
        "defaultValue": false
      },
      {
        "type": "color",
        "messageKey": "ACCENT_COLOR",
        "label": "Color",
        "defaultValue": "FF0000"
      }
    ]
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Date pill"
      },
      {
        "type": "toggle",
        "messageKey": "SHAKE_PILL",
        "label": "Show on shake",
        "defaultValue": true
      },
      {
        "type": "toggle",
        "messageKey": "PILL_BATTERY",
        "label": "Battery on second shake",
        "defaultValue": true
      }
    ]
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Life expectancy"
      },
      {
        "type": "input",
        "messageKey": "BIRTHDAY",
        "label": "Birthday",
        "defaultValue": "1980-01-01",
        "attributes": {
          "type": "date",
          "limit": 10
        }
      },
      {
        "type": "radiogroup",
        "messageKey": "GENDER",
        "label": "Gender",
        "defaultValue": 0,
        "options": [
          { "label": "Male", "value": 0 },
          { "label": "Female", "value": 1 },
          { "label": "Other", "value": 2 }
        ]
      },
      {
        "type": "toggle",
        "messageKey": "USE_LOCATION",
        "label": "Estimate from my location",
        "defaultValue": false,
        "description": "Sends your phone's coordinates to a free lookup " +
            "(BigDataCloud) to work out your country. Off by default — " +
            "pick a country below instead, any time."
      },
      {
        "type": "select",
        "messageKey": "COUNTRY",
        "label": "Country (used when location is off)",
        "defaultValue": "WORLD",
        "options": COUNTRY_OPTIONS
      },
      {
        "type": "toggle",
        "messageKey": "PILL_LIFE",
        "label": "Show weeks-left on shake",
        "defaultValue": true
      }
    ]
  },
  {
    "type": "submit",
    "defaultValue": "Save"
  }
];
