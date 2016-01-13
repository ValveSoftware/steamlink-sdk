// Copyright 2010 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

function TryStart() {
  try {
    var raw_json_data = document.getElementById('json').textContent;
    var json = JSON.parse(raw_json_data);
    var port = chrome.extension.connect();
    port.postMessage({message: 'start', benchmark: json});
  }
  catch(err) {
    console.log("TryStart retrying after exception: " + err);
    setTimeout(TryStart, 1000);
    // TODO(mbelshe): remove me!  This is for debugging.
    console.log("Body is: " + document.body.innerHTML);
    return;
  }

  var status_element = document.getElementById('status');
  status_element.textContent = "ACK";
}

// We wait 1s before starting the test just to let chrome warm up better.
setTimeout(TryStart, 250);
