/******/ (function(modules) { // webpackBootstrap
/******/ 	// The module cache
/******/ 	var installedModules = {};
/******/
/******/ 	// The require function
/******/ 	function __webpack_require__(moduleId) {
/******/
/******/ 		// Check if module is in cache
/******/ 		if(installedModules[moduleId])
/******/ 			return installedModules[moduleId].exports;
/******/
/******/ 		// Create a new module (and put it into the cache)
/******/ 		var module = installedModules[moduleId] = {
/******/ 			exports: {},
/******/ 			id: moduleId,
/******/ 			loaded: false
/******/ 		};
/******/
/******/ 		// Execute the module function
/******/ 		modules[moduleId].call(module.exports, module, module.exports, __webpack_require__);
/******/
/******/ 		// Flag the module as loaded
/******/ 		module.loaded = true;
/******/
/******/ 		// Return the exports of the module
/******/ 		return module.exports;
/******/ 	}
/******/
/******/
/******/ 	// expose the modules object (__webpack_modules__)
/******/ 	__webpack_require__.m = modules;
/******/
/******/ 	// expose the module cache
/******/ 	__webpack_require__.c = installedModules;
/******/
/******/ 	// __webpack_public_path__
/******/ 	__webpack_require__.p = "";
/******/
/******/ 	// Load entry module and return exports
/******/ 	return __webpack_require__(0);
/******/ })
/************************************************************************/
/******/ ([
/* 0 */
/***/ (function(module, exports, __webpack_require__) {

	__webpack_require__(1);
	module.exports = __webpack_require__(2);


/***/ }),
/* 1 */
/***/ (function(module, exports) {

	/**
	 * Copyright 2024 Google LLC
	 *
	 * Licensed under the Apache License, Version 2.0 (the "License");
	 * you may not use this file except in compliance with the License.
	 * You may obtain a copy of the License at
	 *
	 *     http://www.apache.org/licenses/LICENSE-2.0
	 *
	 * Unless required by applicable law or agreed to in writing, software
	 * distributed under the License is distributed on an "AS IS" BASIS,
	 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
	 * See the License for the specific language governing permissions and
	 * limitations under the License.
	 */
	
	(function(p) {
	  if (!p === undefined) {
	    console.error('Pebble object not found!?');
	    return;
	  }
	
	  // Aliases:
	  p.on = p.addEventListener;
	  p.off = p.removeEventListener;
	
	  // For Android (WebView-based) pkjs, print stacktrace for uncaught errors:
	  if (typeof window !== 'undefined' && window.addEventListener) {
	    window.addEventListener('error', function(event) {
	      if (event.error && event.error.stack) {
	        console.error('' + event.error + '\n' + event.error.stack);
	      }
	    });
	  }
	
	})(Pebble);


/***/ }),
/* 2 */
/***/ (function(module, exports) {

	// Listen for when the configuration page is opened
	Pebble.addEventListener('showConfiguration', function() {
	  var url = 'https://rawgit.com/pebble-examples/slate-config-example/master/config/index.html';
	
	  // Build the configuration URL with current settings
	  var settings = JSON.parse(localStorage.getItem('settings')) || {};
	
	  // Create custom configuration page as data URI
	  var configPage = `
	<!DOCTYPE html>
	<html>
	<head>
	  <meta charset="utf-8">
	  <meta name="viewport" content="width=device-width, initial-scale=1">
	  <title>4K Weeks Settings</title>
	  <style>
	    body {
	      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
	      margin: 0;
	      padding: 20px;
	      background: #f5f5f5;
	    }
	    .container {
	      max-width: 500px;
	      margin: 0 auto;
	      background: white;
	      padding: 20px;
	      border-radius: 8px;
	      box-shadow: 0 2px 10px rgba(0,0,0,0.1);
	    }
	    h1 {
	      margin-top: 0;
	      color: #333;
	    }
	    label {
	      display: block;
	      margin-top: 15px;
	      margin-bottom: 5px;
	      font-weight: 600;
	      color: #555;
	    }
	    input, select {
	      width: 100%;
	      padding: 10px;
	      border: 1px solid #ddd;
	      border-radius: 4px;
	      font-size: 16px;
	      box-sizing: border-box;
	    }
	    button {
	      margin-top: 20px;
	      width: 100%;
	      padding: 12px;
	      background: #2196F3;
	      color: white;
	      border: none;
	      border-radius: 4px;
	      font-size: 16px;
	      font-weight: 600;
	      cursor: pointer;
	    }
	    button:active {
	      background: #1976D2;
	    }
	    .info {
	      margin-top: 15px;
	      padding: 10px;
	      background: #e3f2fd;
	      border-radius: 4px;
	      font-size: 14px;
	      color: #1565c0;
	    }
	  </style>
	</head>
	<body>
	  <div class="container">
	    <h1>4K Weeks Settings</h1>
	
	    <label for="birth-year">Birth Year</label>
	    <input type="number" id="birth-year" min="1900" max="2026" value="${settings.birthYear || 1990}">
	
	    <label for="birth-month">Birth Month</label>
	    <select id="birth-month">
	      <option value="1">January</option>
	      <option value="2">February</option>
	      <option value="3">March</option>
	      <option value="4">April</option>
	      <option value="5">May</option>
	      <option value="6">June</option>
	      <option value="7">July</option>
	      <option value="8">August</option>
	      <option value="9">September</option>
	      <option value="10">October</option>
	      <option value="11">November</option>
	      <option value="12">December</option>
	    </select>
	
	    <label for="birth-day">Birth Day</label>
	    <input type="number" id="birth-day" min="1" max="31" value="${settings.birthDay || 1}">
	
	    <label for="gender">Gender</label>
	    <select id="gender">
	      <option value="0">Male</option>
	      <option value="1">Female</option>
	      <option value="2">Other</option>
	    </select>
	
	    <label for="country">Country</label>
	    <select id="country">
	      <option value="80">Global Average (80 years)</option>
	      <option value="85">Japan (85 years)</option>
	      <option value="83">Switzerland (83 years)</option>
	      <option value="82">Australia (82 years)</option>
	      <option value="82">Spain (82 years)</option>
	      <option value="81">Italy (81 years)</option>
	      <option value="81">Canada (81 years)</option>
	      <option value="79">United States (79 years)</option>
	      <option value="78">United Kingdom (78 years)</option>
	      <option value="77">China (77 years)</option>
	      <option value="73">India (73 years)</option>
	    </select>
	
	    <div class="info">
	      Life expectancy values are approximate averages and may vary by gender.
	    </div>
	
	    <button id="save-button">Save Settings</button>
	  </div>
	
	  <script>
	    // Restore saved values
	    var settings = ${JSON.stringify(settings)};
	    if (settings.birthMonth) {
	      document.getElementById('birth-month').value = settings.birthMonth;
	    }
	    if (settings.gender !== undefined) {
	      document.getElementById('gender').value = settings.gender;
	    }
	    if (settings.lifeExpectancy) {
	      document.getElementById('country').value = settings.lifeExpectancy;
	    }
	
	    document.getElementById('save-button').addEventListener('click', function() {
	      var options = {
	        birthYear: parseInt(document.getElementById('birth-year').value),
	        birthMonth: parseInt(document.getElementById('birth-month').value),
	        birthDay: parseInt(document.getElementById('birth-day').value),
	        gender: parseInt(document.getElementById('gender').value),
	        lifeExpectancy: parseInt(document.getElementById('country').value)
	      };
	
	      // Send settings to watch
	      var location = 'pebblejs://close#' + encodeURIComponent(JSON.stringify(options));
	      window.location = location;
	    });
	  </script>
	</body>
	</html>
	  `;
	
	  Pebble.openURL('data:text/html;charset=utf-8,' + encodeURIComponent(configPage));
	});
	
	// Listen for when the configuration page is closed
	Pebble.addEventListener('webviewclosed', function(e) {
	  if (e && e.response) {
	    var options = JSON.parse(decodeURIComponent(e.response));
	
	    // Save settings to localStorage
	    localStorage.setItem('settings', JSON.stringify(options));
	
	    // Send settings to watch
	    Pebble.sendAppMessage(options, function() {
	      console.log('Settings sent successfully');
	    }, function() {
	      console.log('Failed to send settings');
	    });
	  }
	});


/***/ })
/******/ ]);
//# sourceMappingURL=pebble-js-app.js.map