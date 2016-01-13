// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

<include src="../../../ui/webui/resources/js/util.js"></include>

function reload() {
  if ($('helpTooltip'))
    return;
  history.go(0);
}

function formatNumber(str) {
  str += '';
  if (str == '0') {
    return 'N/A ';
  }
  var x = str.split('.');
  var x1 = x[0];
  var x2 = x.length > 1 ? '.' + x[1] : '';
  var regex = /(\d+)(\d{3})/;
  while (regex.test(x1)) {
    x1 = x1.replace(regex, '$1' + ',' + '$2');
  }
  return x1;
}

function addToSum(id, value) {
  var target = document.getElementById(id);
  var sum = parseInt(target.innerHTML);
  sum += parseInt(value);
  target.innerHTML = sum;
}

function handleHelpTooltipMouseOver(event) {
  var el = document.createElement('div');
  el.id = 'helpTooltip';
  el.innerHTML = event.toElement.getElementsByTagName('div')[0].innerHTML;
  el.style.top = 0;
  el.style.left = 0;
  el.style.visibility = 'hidden';
  document.body.appendChild(el);

  var width = el.offsetWidth;
  var height = el.offsetHeight;

  var scrollLeft = scrollLeftForDocument(document);
  if (event.pageX - width - 50 + scrollLeft >= 0)
    el.style.left = (event.pageX - width - 20) + 'px';
  else
    el.style.left = (event.pageX + 20) + 'px';

  var scrollTop = scrollTopForDocument(document);
  if (event.pageY - height - 50 + scrollTop >= 0)
    el.style.top = (event.pageY - height - 20) + 'px';
  else
    el.style.top = (event.pageY + 20) + 'px';

  el.style.visibility = 'visible';
}

function handleHelpTooltipMouseOut(event) {
  var el = $('helpTooltip');
  el.parentNode.removeChild(el);
}

function enableHelpTooltips() {
  var helpEls = document.getElementsByClassName('help');

  for (var i = 0, helpEl; helpEl = helpEls[i]; i++) {
    helpEl.onmouseover = handleHelpTooltipMouseOver;
    helpEl.onmouseout = handleHelpTooltipMouseOut;
  }
}

document.addEventListener('DOMContentLoaded', function() {
  // This is the javascript code that processes the template:
  var input = new JsEvalContext(loadTimeData.getValue('jstemplateData'));
  var output = $('t');
  jstProcess(input, output);

  enableHelpTooltips();
});

