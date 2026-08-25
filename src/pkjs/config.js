// Country options carry the same codes as src/c/life_expectancy.h's table,
// looked up there by string match, so this list's order is free to serve the
// picker instead: alphabetical by label, with World average pinned first
// since it's the default. life_expectancy.h keeps WORLD last instead — its
// fallback logic hardcodes "last entry" — an unrelated, C-side-only need.
var COUNTRY_OPTIONS = [
  { "label": "World average", "value": "WORLD" },
  { "label": "Argentina", "value": "AR" },
  { "label": "Australia", "value": "AU" },
  { "label": "Austria", "value": "AT" },
  { "label": "Bangladesh", "value": "BD" },
  { "label": "Belgium", "value": "BE" },
  { "label": "Brazil", "value": "BR" },
  { "label": "Bulgaria", "value": "BG" },
  { "label": "Canada", "value": "CA" },
  { "label": "Chile", "value": "CL" },
  { "label": "China", "value": "CN" },
  { "label": "Colombia", "value": "CO" },
  { "label": "Croatia", "value": "HR" },
  { "label": "Czechia", "value": "CZ" },
  { "label": "Denmark", "value": "DK" },
  { "label": "Egypt", "value": "EG" },
  { "label": "Ethiopia", "value": "ET" },
  { "label": "Finland", "value": "FI" },
  { "label": "France", "value": "FR" },
  { "label": "Germany", "value": "DE" },
  { "label": "Ghana", "value": "GH" },
  { "label": "Greece", "value": "GR" },
  { "label": "Hungary", "value": "HU" },
  { "label": "Iceland", "value": "IS" },
  { "label": "India", "value": "IN" },
  { "label": "Indonesia", "value": "ID" },
  { "label": "Ireland", "value": "IE" },
  { "label": "Israel", "value": "IL" },
  { "label": "Italy", "value": "IT" },
  { "label": "Japan", "value": "JP" },
  { "label": "Kenya", "value": "KE" },
  { "label": "Malaysia", "value": "MY" },
  { "label": "Mexico", "value": "MX" },
  { "label": "Netherlands", "value": "NL" },
  { "label": "New Zealand", "value": "NZ" },
  { "label": "Nigeria", "value": "NG" },
  { "label": "Norway", "value": "NO" },
  { "label": "Pakistan", "value": "PK" },
  { "label": "Peru", "value": "PE" },
  { "label": "Philippines", "value": "PH" },
  { "label": "Poland", "value": "PL" },
  { "label": "Portugal", "value": "PT" },
  { "label": "Romania", "value": "RO" },
  { "label": "Russia", "value": "RU" },
  { "label": "Saudi Arabia", "value": "SA" },
  { "label": "Singapore", "value": "SG" },
  { "label": "Slovakia", "value": "SK" },
  { "label": "Slovenia", "value": "SI" },
  { "label": "South Africa", "value": "ZA" },
  { "label": "South Korea", "value": "KR" },
  { "label": "Spain", "value": "ES" },
  { "label": "Sweden", "value": "SE" },
  { "label": "Switzerland", "value": "CH" },
  { "label": "Thailand", "value": "TH" },
  { "label": "Turkey", "value": "TR" },
  { "label": "Ukraine", "value": "UA" },
  { "label": "United Arab Emirates", "value": "AE" },
  { "label": "United Kingdom", "value": "GB" },
  { "label": "United States", "value": "US" },
  { "label": "Vietnam", "value": "VN" }
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
        "description": "Selects which population life expectancy figure to use — " +
            "the only two that exist in the data. Non-binary / prefer not to say " +
            "averages the two rather than assuming either.",
        "options": [
          { "label": "Male", "value": 0 },
          { "label": "Female", "value": 1 },
          { "label": "Non-binary / prefer not to say", "value": 2 }
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
