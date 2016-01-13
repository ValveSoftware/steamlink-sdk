/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

var ui = ui || {};

(function () {

ui.displayURLForBuilder = function(builderName)
{
    return config.waterfallURL + '?' + $.param({
        'builder': builderName
    });
}

ui.kUseNewWindowForLinksSetting = 'gardenomatic.use-new-window-for-links';

ui.displayNameForBuilder = function(builderName)
{
    return builderName.replace(/Webkit /, '');
}

ui.urlForTest = function(testName)
{
    return 'http://trac.webkit.org/browser/trunk/LayoutTests/' + testName;
}

ui.urlForCrbug = function(bugID)
{
    return 'http://crbug.com/' + bugID;
}

ui.urlForFlakinessDashboard = function(opt_testNameList)
{
    var testsParameter = opt_testNameList ? opt_testNameList.join(',') : '';
    return 'http://test-results.appspot.com/dashboards/flakiness_dashboard.html#tests=' + encodeURIComponent(testsParameter);
}

ui.urlForEmbeddedFlakinessDashboard = function(opt_testNameList)
{
    return ui.urlForFlakinessDashboard(opt_testNameList) + '&showChrome=false';
}

ui.rolloutReasonForTestNameList = function(testNameList)
{
    return 'Broke:\n' + testNameList.map(function(testName) {
        return '* ' + testName;
    }).join('\n');
}

ui.setTargetForLink = function(anchor)
{
    if (anchor.href.indexOf('#') === 0)
        return;
    if (ui.useNewWindowForLinks)
        anchor.target = '_blank';
    else
        anchor.removeAttribute('target');
}

ui.setUseNewWindowForLinks = function(enabled)
{
    ui.useNewWindowForLinks = enabled;
    if (enabled)
        localStorage[ui.kUseNewWindowForLinksSetting] = 'true';
    else
        delete localStorage[ui.kUseNewWindowForLinksSetting];

    $('a').each(function() {
        ui.setTargetForLink(this);
    });
}
ui.setUseNewWindowForLinks(!!localStorage[ui.kUseNewWindowForLinksSetting]);

ui.createLinkNode = function(url, textContent)
{
    var link = document.createElement('a');
    link.href = url;
    ui.setTargetForLink(link);
    link.appendChild(document.createTextNode(textContent));
    return link;
}

ui.onebar = base.extends('div', {
    init: function()
    {
        this.id = 'onebar';
        this.innerHTML =
            '<ul>' +
                '<li><a href="#unexpected">Unexpected Failures</a></li>' +
                '<li><a href="#expected">Expected Failures</a></li>' +
                '<li><a href="#results">Results</a></li>' +
            '</ul>' +
            '<div id="link-handling"><input type="checkbox" id="new-window-for-links"><label for="new-window-for-links">Open links in new window</label></div>' +
            '<div id="unexpected"></div>' +
            '<div id="expected"></div>' +
            '<div id="results"></div>';
        this._tabNames = [
            'unexpected',
            'expected',
            'results',
        ]

        this._tabIndexToSavedScrollOffset = {};
        this._tabs = $(this).tabs({
            disabled: [2],
            show: function(event, ui) { this._restoreScrollOffset(ui.index); },
            select: function(event, ui) {
                this._saveScrollOffset();
                window.location.hash = ui.tab.hash;
            }.bind(this)
        });
    },
    _saveScrollOffset: function() {
        var tabIndex = this._tabs.tabs('option', 'selected');
        this._tabIndexToSavedScrollOffset[tabIndex] = document.body.scrollTop;
    },
    _restoreScrollOffset: function(tabIndex)
    {
        document.body.scrollTop = this._tabIndexToSavedScrollOffset[tabIndex] || 0;
    },
    _setupHistoryHandlers: function()
    {
        function currentHash() {
            var hash = window.location.hash;
            return (!hash || hash == '#') ? '#unexpected' : hash;
        }

        var self = this;
        window.onhashchange = function(event) {
            var tabName = currentHash().substring(1);
            self._selectInternal(tabName);
        };

        // When navigating from the browser chrome, we'll
        // scroll to the #tabname contents. popstate fires before
        // we scroll, so we can save the scroll offset first.
        window.onpopstate = function() {
            self._saveScrollOffset();
        };
    },
    _setupLinkSettingHandler: function()
    {
        $('#new-window-for-links').attr('checked', ui.useNewWindowForLinks);
        $('#new-window-for-links').change(function(event) {
            ui.setUseNewWindowForLinks(this.checked);
        });
    },
    attach: function()
    {
        document.body.insertBefore(this, document.body.firstChild);
        this._setupLinkSettingHandler();
        this._setupHistoryHandlers();
    },
    tabNamed: function(tabName)
    {
        if (this._tabNames.indexOf(tabName) == -1)
            return null;
        tab = document.getElementById(tabName);
        // We perform this sanity check below to make sure getElementById
        // hasn't given us a node in some other unrelated part of the document.
        // that shouldn't happen normally, but it could happen if an attacker
        // has somehow sneakily added a node to our document.
        if (tab.parentNode != this)
            return null;
        return tab;
    },
    unexpected: function()
    {
        return this.tabNamed('unexpected');
    },
    expected: function()
    {
        return this.tabNamed('expected');
    },
    results: function()
    {
        return this.tabNamed('results');
    },
    _selectInternal: function(tabName) {
        var tabIndex = this._tabNames.indexOf(tabName);
        this._tabs.tabs('enable', tabIndex);
        this._tabs.tabs('select', tabIndex);
    },
    select: function(tabName)
    {
        this._saveScrollOffset();
        this._selectInternal(tabName);
        window.location = '#' + tabName;
    }
});

ui.TreeStatus = base.extends('div',  {
    addStatus: function(name)
    {
        var label = document.createElement('div');
        label.textContent = " " + name + ' status: ';
        this.appendChild(label);
        var statusSpan = document.createElement('span');
        statusSpan.textContent = '(Loading...) ';
        label.appendChild(statusSpan);
        treestatus.fetchTreeStatus(treestatus.urlByName(name), statusSpan);
    },
    init: function()
    {
        this.className = 'treestatus';
        this.addStatus('blink');
        this.addStatus('chromium');
    },
});

ui.StatusArea = base.extends('div',  {
    init: function()
    {
        // This is a Singleton.
        if (ui.StatusArea._instance)
            return ui.StatusArea._instance;
        ui.StatusArea._instance = this;

        var kMinimumStatusAreaHeightPx = 60;
        var dragger = document.createElement('div');
        var initialY;
        var initialHeight;
        dragger.className = 'dragger';
        $(dragger).mousedown(function(e) {
            initialY = e.pageY;
            initialHeight = $(this).height();
            $(document.body).addClass('status-resizing');
        }.bind(this));
        $(document.body).mouseup(function(e) {
            initialY = 0;
            initialHeight = 0;
            $(document.body).removeClass('status-resizing');
        });
        $(document.body).mousemove(function(e) {
            if (initialY) {
                var newHeight = initialHeight + initialY - e.pageY;
                if (newHeight >= kMinimumStatusAreaHeightPx)
                    $(this).height(newHeight);
                e.preventDefault();
            }
        }.bind(this));
        this.appendChild(dragger);

        this.contents = document.createElement('div');
        this.contents.className = 'contents';
        this.appendChild(this.contents);

        this.className = 'status';
        document.body.appendChild(this);
        this._currentId = 0;
        this._unfinishedIds = {};

        this.appendChild(new ui.actions.List([new ui.actions.Close()]));
        $(this).bind('close', this.close.bind(this));

        var processing = document.createElement('progress');
        processing.className = 'process-text';
        processing.textContent = 'Processing...';
        this.appendChild(processing);
    },
    close: function()
    {
        this.style.visibility = 'hidden';
        Array.prototype.forEach.call(this.querySelectorAll('.status-content'), function(node) {
            node.parentNode.removeChild(node);
        });
    },
    addMessage: function(id, message)
    {
        this.style.visibility = 'visible';
        $(this).addClass('processing');

        var element = document.createElement('div');
        $(element).addClass('message').text(message);

        var content = this.querySelector('#' + id);
        if (!content) {
            content = document.createElement('div');
            content.id = id;
            content.className = 'status-content';
            this.contents.appendChild(content);
        }

        content.appendChild(element);
        if (element.offsetTop < this.scrollTop || element.offsetTop + element.offsetHeight > this.scrollTop + this.offsetHeight)
            this.scrollTop = element.offsetTop;
    },
    // FIXME: It's unclear whether this code could live here or in a controller.
    addFinalMessage: function(id, message)
    {
        this.addMessage(id, message);

        delete this._unfinishedIds[id];
        if (!Object.keys(this._unfinishedIds).length)
            $(this).removeClass('processing');
    },
    newId: function() {
        var id = 'status-content-' + ++this._currentId;
        this._unfinishedIds[id] = 1;
        return id;
    }
});

ui.revisionDetails = base.extends('span', {
    // We only support 2 levels of visual escalation levels: warning and critical.
    warnRollRevisionSpanThreshold: 45,
    criticalRollRevisionSpanThreshold: 80,
    classNameForUrgencyLevel: function(rollRevisionSpan) {
        if (rollRevisionSpan < this.criticalRollRevisionSpanThreshold)
            return "warning";
        return "critical";
    },
    updateUI: function(totRevision) {
        this.appendChild(document.createElement("br"));
        this.appendChild(document.createTextNode('Last roll is to '));
        this.appendChild(ui.createLinkNode(trac.changesetURL(this.lastRolledRevision), this.lastRolledRevision));
        var rollRevisionSpan = totRevision - this.lastRolledRevision;
        // Don't clutter the UI if we haven't run behind.
        if (rollRevisionSpan > this.warnRollRevisionSpanThreshold) {
            var wrapper = document.createElement("span");
            wrapper.className = this.classNameForUrgencyLevel(rollRevisionSpan);
            wrapper.appendChild(document.createTextNode("(" + rollRevisionSpan + " revisions behind)"));
            this.appendChild(wrapper);
        }
        this.appendChild(document.createTextNode(', current autoroll '));
        if (this.roll) {
            var linkText = "" + this.roll.fromRevision + ":" + this.roll.toRevision;
            this.appendChild(ui.createLinkNode(this.roll.url, linkText));
            if (this.roll.isStopped)
                this.appendChild(document.createTextNode(' (STOPPED) '));
        } else {
            this.appendChild(document.createTextNode(' None'));
        }
    },
    init: function() {
        var theSpan = this;
        theSpan.appendChild(document.createTextNode('Latest revision processed by every bot: '));

        var latestRevision = model.latestRevisionWithNoBuildersInFlight();
        var latestRevisions = model.latestRevisionByBuilder();

        // Get the list of builders sorted with the most recent one first.
        var builders = Object.keys(latestRevisions);
        builders.sort(function (a, b) { return parseInt(latestRevisions[b]) - parseInt(latestRevisions[a]);});

        var summaryNode = document.createElement('summary');
        var summaryLinkNode = ui.createLinkNode(trac.changesetURL(latestRevision), latestRevision);
        summaryNode.appendChild(summaryLinkNode);

        var revisionsTableNode = document.createElement('table');
        builders.forEach(function(builderName) {
            var trNode = document.createElement('tr');

            var tdNode = document.createElement('td');
            tdNode.appendChild(ui.createLinkNode(ui.displayURLForBuilder(builderName), builderName.replace('WebKit ', '')));
            trNode.appendChild(tdNode);

            var tdNode = document.createElement('td');
            tdNode.appendChild(document.createTextNode(latestRevisions[builderName]));
            trNode.appendChild(tdNode);

            revisionsTableNode.appendChild(trNode);
        });

        var revisionsNode = document.createElement('details');
        revisionsNode.appendChild(summaryNode);
        revisionsNode.appendChild(revisionsTableNode);
        theSpan.appendChild(revisionsNode);

        // This adds a pop-up when we hover over the summary if the details aren't being shown.
        var revisionsPopUp = $('<span id="revisionPopUp">').appendTo(summaryLinkNode);
        revisionsPopUp.append($(revisionsTableNode).clone());
        $(summaryLinkNode).mouseover(function(ev) {
            if (!revisionsNode.open) {
                var tPosX = $(summaryNode).position().left;
                var tPosY = $(summaryNode).position().top + 16;
                $(revisionsPopUp).css({'position': 'absolute', 'top': tPosY, 'left': tPosX});
                $(revisionsPopUp).addClass('active');
            }
        });
        $(summaryLinkNode).mouseout(function(ev) {
            if (!revisionsNode.open) {
                $(revisionsPopUp).removeClass("active");
            }
        });

        var totRevision = model.latestRevision();
        theSpan.appendChild(document.createTextNode(', trunk is at '));
        theSpan.appendChild(ui.createLinkNode(trac.changesetURL(totRevision), totRevision));

        Promise.all([checkout.lastBlinkRollRevision(), rollbot.fetchCurrentRoll()]).then(function(results) {
            theSpan.lastRolledRevision = results[0];
            theSpan.roll = results[1];
            theSpan.updateUI(totRevision);
        });
    }
});

})();
