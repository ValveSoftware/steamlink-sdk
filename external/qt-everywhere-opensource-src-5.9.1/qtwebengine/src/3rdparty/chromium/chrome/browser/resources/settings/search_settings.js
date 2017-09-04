// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings', function() {
  /** @const {string} */
  var WRAPPER_CSS_CLASS = 'search-highlight-wrapper';

  /** @const {string} */
  var ORIGINAL_CONTENT_CSS_CLASS = 'search-highlight-original-content';

  /** @const {string} */
  var HIT_CSS_CLASS = 'search-highlight-hit';

  /** @const {string} */
  var SEARCH_BUBBLE_CSS_CLASS = 'search-bubble';

  /**
   * A CSS attribute indicating that a node should be ignored during searching.
   * @const {string}
   */
  var SKIP_SEARCH_CSS_ATTRIBUTE = 'no-search';

  /**
   * List of elements types that should not be searched at all.
   * The only DOM-MODULE node is in <body> which is not searched, therefore
   * DOM-MODULE is not needed in this set.
   * @const {!Set<string>}
   */
  var IGNORED_ELEMENTS = new Set([
    'CONTENT',
    'CR-EVENTS',
    'IMG',
    'IRON-ICON',
    'IRON-LIST',
    'PAPER-ICON-BUTTON',
    /* TODO(dpapad): paper-item is used for dynamically populated dropdown
     * menus. Perhaps a better approach is to mark the entire dropdown menu such
     * that search algorithm can skip it as a whole instead.
     */
    'PAPER-ITEM',
    'PAPER-RIPPLE',
    'PAPER-SLIDER',
    'PAPER-SPINNER',
    'STYLE',
    'TEMPLATE',
  ]);

  /**
   * Finds all previous highlighted nodes under |node| (both within self and
   * children's Shadow DOM) and replaces the highlights (yellow rectangle and
   * search bubbles) with the original text node.
   * TODO(dpapad): Consider making this a private method of TopLevelSearchTask.
   * @param {!Node} node
   * @private
   */
  function findAndRemoveHighlights_(node) {
    var wrappers = node.querySelectorAll('* /deep/ .' + WRAPPER_CSS_CLASS);

    for (var wrapper of wrappers) {
      var originalNode = wrapper.querySelector(
          '.' + ORIGINAL_CONTENT_CSS_CLASS);
      wrapper.parentElement.replaceChild(originalNode.firstChild, wrapper);
    }

    var searchBubbles = node.querySelectorAll(
        '* /deep/ .' + SEARCH_BUBBLE_CSS_CLASS);
    for (var searchBubble of searchBubbles)
      searchBubble.remove();
  }

  /**
   * Applies the highlight UI (yellow rectangle) around all matches in |node|.
   * @param {!Node} node The text node to be highlighted. |node| ends up
   *     being removed from the DOM tree.
   * @param {!Array<string>} tokens The string tokens after splitting on the
   *     relevant regExp. Even indices hold text that doesn't need highlighting,
   *     odd indices hold the text to be highlighted. For example:
   *     var r = new RegExp('(foo)', 'i');
   *     'barfoobar foo bar'.split(r) => ['bar', 'foo', 'bar ', 'foo', ' bar']
   * @private
   */
  function highlight_(node, tokens) {
    var wrapper = document.createElement('span');
    wrapper.classList.add(WRAPPER_CSS_CLASS);
    // Use existing node as placeholder to determine where to insert the
    // replacement content.
    node.parentNode.replaceChild(wrapper, node);

    // Keep the existing node around for when the highlights are removed. The
    // existing text node might be involved in data-binding and therefore should
    // not be discarded.
    var span = document.createElement('span');
    span.classList.add(ORIGINAL_CONTENT_CSS_CLASS);
    span.style.display = 'none';
    span.appendChild(node);
    wrapper.appendChild(span);

    for (var i = 0; i < tokens.length; ++i) {
      if (i % 2 == 0) {
        wrapper.appendChild(document.createTextNode(tokens[i]));
      } else {
        var span = document.createElement('span');
        span.classList.add(HIT_CSS_CLASS);
        span.style.backgroundColor = '#ffeb3b'; // --var(--paper-yellow-500)
        span.textContent = tokens[i];
        wrapper.appendChild(span);
      }
    }
  }

  /**
   * Traverses the entire DOM (including Shadow DOM), finds text nodes that
   * match the given regular expression and applies the highlight UI. It also
   * ensures that <settings-section> instances become visible if any matches
   * occurred under their subtree.
   *
   * @param {!settings.SearchRequest} request
   * @param {!Node} root The root of the sub-tree to be searched
   * @private
   */
  function findAndHighlightMatches_(request, root) {
    var foundMatches = false;
    function doSearch(node) {
      if (node.nodeName == 'TEMPLATE' && node.hasAttribute('route-path') &&
          !node.if && !node.hasAttribute(SKIP_SEARCH_CSS_ATTRIBUTE)) {
        getSearchManager().queue_.addRenderTask(
            new RenderTask(request, node));
        return;
      }

      if (IGNORED_ELEMENTS.has(node.nodeName))
        return;

      if (node instanceof HTMLElement) {
        var element = /** @type {HTMLElement} */(node);
        if (element.hasAttribute(SKIP_SEARCH_CSS_ATTRIBUTE) ||
            element.hasAttribute('hidden') ||
            element.style.display == 'none') {
          return;
        }
      }

      if (node.nodeType == Node.TEXT_NODE) {
        var textContent = node.nodeValue.trim();
        if (textContent.length == 0)
          return;

        if (request.regExp.test(textContent)) {
          foundMatches = true;
          revealParentSection_(node, request.rawQuery_);

          // Don't highlight <select> nodes, yellow rectangles can't be
          // displayed within an <option>.
          // TODO(dpapad): highlight <select> controls with a search bubble
          // instead.
          if (node.parentNode.nodeName != 'OPTION')
            highlight_(node, textContent.split(request.regExp));
        }
        // Returning early since TEXT_NODE nodes never have children.
        return;
      }

      var child = node.firstChild;
      while (child !== null) {
        // Getting a reference to the |nextSibling| before calling doSearch()
        // because |child| could be removed from the DOM within doSearch().
        var nextSibling = child.nextSibling;
        doSearch(child);
        child = nextSibling;
      }

      var shadowRoot = node.shadowRoot;
      if (shadowRoot)
        doSearch(shadowRoot);
    }

    doSearch(root);
    return foundMatches;
  }

  /**
   * Highlights the HTML control that triggers a subpage, by displaying a search
   * bubble.
   * @param {!HTMLElement} element The element to be highlighted.
   * @param {string} rawQuery The search query.
   * @private
   */
  function highlightAssociatedControl_(element, rawQuery) {
    var searchBubble = element.querySelector('.' + SEARCH_BUBBLE_CSS_CLASS);
    // If the associated control has already been highlighted due to another
    // match on the same subpage, there is no need to do anything.
    if (searchBubble)
      return;

    searchBubble = document.createElement('div');
    searchBubble.classList.add(SEARCH_BUBBLE_CSS_CLASS);
    var innards = document.createElement('div');
    innards.classList.add('search-bubble-innards');
    innards.textContent = rawQuery;
    searchBubble.appendChild(innards);
    element.appendChild(searchBubble);

    // Dynamically position the bubble at the edge the associated control
    // element.
    var updatePosition = function() {
      if (innards.classList.contains('above')) {
        searchBubble.style.top =
            element.offsetTop - searchBubble.offsetHeight + 'px';
      } else {
        searchBubble.style.top =
            element.offsetTop + element.offsetHeight + 'px';
      }
    };
    updatePosition();

    searchBubble.addEventListener('mouseover', function() {
      innards.classList.toggle('above');
      updatePosition();
    });
  }

  /**
   * Finds and makes visible the <settings-section> parent of |node|.
   * @param {!Node} node
   * @param {string} rawQuery
   * @private
   */
  function revealParentSection_(node, rawQuery) {
    var associatedControl = null;
    // Find corresponding SETTINGS-SECTION parent and make it visible.
    var parent = node;
    while (parent && parent.nodeName !== 'SETTINGS-SECTION') {
      parent = parent.nodeType == Node.DOCUMENT_FRAGMENT_NODE ?
          parent.host : parent.parentNode;
      if (parent.nodeName == 'SETTINGS-SUBPAGE') {
        // TODO(dpapad): Cast to SettingsSubpageElement here.
        associatedControl = assert(
            parent.associatedControl,
            'An associated control was expected for SETTINGS-SUBPAGE ' +
                parent.pageTitle + ', but was not found.');
      }
    }
    if (parent)
      parent.hiddenBySearch = false;

    // Need to add the search bubble after the parent SETTINGS-SECTION has
    // become visible, otherwise |offsetWidth| returns zero.
    if (associatedControl)
      highlightAssociatedControl_(associatedControl, rawQuery);
  }

  /**
   * @constructor
   *
   * @param {!settings.SearchRequest} request
   * @param {!Node} node
   */
  function Task(request, node) {
    /** @protected {!settings.SearchRequest} */
    this.request = request;

    /** @protected {!Node} */
    this.node = node;
  }

  Task.prototype = {
    /**
     * @abstract
     * @return {!Promise}
     */
    exec: function() {},
  };

  /**
   * A task that takes a <template is="dom-if">...</template> node corresponding
   * to a setting subpage and renders it. A SearchAndHighlightTask is posted for
   * the newly rendered subtree, once rendering is done.
   * @constructor
   * @extends {Task}
   *
   * @param {!settings.SearchRequest} request
   * @param {!Node} node
   */
  function RenderTask(request, node) {
    Task.call(this, request, node);
  }

  RenderTask.prototype = {
    /** @override */
    exec: function() {
      var routePath = this.node.getAttribute('route-path');
      var subpageTemplate =
          this.node['_content'].querySelector('settings-subpage');
      subpageTemplate.setAttribute('route-path', routePath);
      assert(!this.node.if);
      this.node.if = true;

      return new Promise(function(resolve, reject) {
        var parent = this.node.parentNode;
        parent.async(function() {
          var renderedNode =
              parent.querySelector('[route-path="' + routePath + '"]');
          // Register a SearchAndHighlightTask for the part of the DOM that was
          // just rendered.
          getSearchManager().queue_.addSearchAndHighlightTask(
              new SearchAndHighlightTask(this.request, assert(renderedNode)));
          resolve();
        }.bind(this));
      }.bind(this));
    },
  };

  /**
   * @constructor
   * @extends {Task}
   *
   * @param {!settings.SearchRequest} request
   * @param {!Node} node
   */
  function SearchAndHighlightTask(request, node) {
    Task.call(this, request, node);
  }

  SearchAndHighlightTask.prototype = {
    /** @override */
    exec: function() {
      var foundMatches = findAndHighlightMatches_(this.request, this.node);
      this.request.updateMatches(foundMatches);
      return Promise.resolve();
    },
  };

  /**
   * @constructor
   * @extends {Task}
   *
   * @param {!settings.SearchRequest} request
   * @param {!Node} page
   */
  function TopLevelSearchTask(request, page) {
    Task.call(this, request, page);
  }

  TopLevelSearchTask.prototype = {
    /** @override */
    exec: function() {
      findAndRemoveHighlights_(this.node);

      var shouldSearch = this.request.regExp !== null;
      this.setSectionsVisibility_(!shouldSearch);
      if (shouldSearch) {
        var foundMatches = findAndHighlightMatches_(this.request, this.node);
        this.request.updateMatches(foundMatches);
      }

      return Promise.resolve();
    },

    /**
     * @param {boolean} visible
     * @private
     */
    setSectionsVisibility_: function(visible) {
      var sections = Polymer.dom(
          this.node.root).querySelectorAll('settings-section');
      for (var i = 0; i < sections.length; i++)
        sections[i].hiddenBySearch = !visible;
    },
  };

  /**
   * @constructor
   */
  function TaskQueue() {
    /**
     * @private {{
     *   high: !Array<!Task>,
     *   middle: !Array<!Task>,
     *   low: !Array<!Task>
     * }}
     */
    this.queues_;
    this.reset();

    /** @private {?Function} */
    this.onEmptyCallback_ = null;

    /**
     * Whether a task is currently running.
     * @private {boolean}
     */
    this.running_ = false;
  }

  TaskQueue.prototype = {
    /** Drops all tasks. */
    reset: function() {
      this.queues_ = {high: [], middle: [], low: []};
    },

    /** @param {!TopLevelSearchTask} task */
    addTopLevelSearchTask: function(task) {
      this.queues_.high.push(task);
      this.consumePending_();
    },

    /** @param {!SearchAndHighlightTask} task */
    addSearchAndHighlightTask: function(task) {
      this.queues_.middle.push(task);
      this.consumePending_();
    },

    /** @param {!RenderTask} task */
    addRenderTask: function(task) {
      this.queues_.low.push(task);
      this.consumePending_();
    },

    /**
     * Registers a callback to be called every time the queue becomes empty.
     * @param {function():void} onEmptyCallback
     */
    onEmpty: function(onEmptyCallback) {
      this.onEmptyCallback_ = onEmptyCallback;
    },

    /**
     * @return {!Task|undefined}
     * @private
     */
    popNextTask_: function() {
      return this.queues_.high.shift() ||
          this.queues_.middle.shift() ||
          this.queues_.low.shift();
    },

    /** @private */
    consumePending_: function() {
      if (this.running_)
        return;

      while (1) {
        var task = this.popNextTask_();
        if (!task) {
          this.running_ = false;
          if (this.onEmptyCallback_)
            this.onEmptyCallback_();
          return;
        }

        this.running_ = true;
        window.requestIdleCallback(function() {
          function startNextTask() {
            this.running_ = false;
            this.consumePending_();
          }
          if (task.request.id ==
              getSearchManager().activeRequest_.id) {
            task.exec().then(startNextTask.bind(this));
          } else {
            // Dropping this task without ever executing it, since a new search
            // has been issued since this task was queued.
            startNextTask.call(this);
          }
        }.bind(this));
        return;
      }
    },
  };

  /**
   * @constructor
   */
  var SearchRequest = function(rawQuery) {
    /** @type {number} */
    this.id = SearchRequest.nextId_++;

    /** @private {string} */
    this.rawQuery_ = rawQuery;

    /** @type {?RegExp} */
    this.regExp = this.generateRegExp_();

    /**
     * Whether this request was fully processed.
     * @type {boolean}
     */
    this.finished = false;

    /** @private {boolean} */
    this.foundMatches_ = false;

    /** @type {!PromiseResolver} */
    this.resolver = new PromiseResolver();
  };

  /** @private {number} */
  SearchRequest.nextId_ = 0;

  /** @private {!RegExp} */
  SearchRequest.SANITIZE_REGEX_ = /[-[\]{}()*+?.,\\^$|#\s]/g;

  SearchRequest.prototype = {
    /**
     * @return {?RegExp}
     * @private
     */
    generateRegExp_: function() {
      var regExp = null;

      // Generate search text by escaping any characters that would be
      // problematic for regular expressions.
      var searchText = this.rawQuery_.trim().replace(
          SearchRequest.SANITIZE_REGEX_, '\\$&');
      if (searchText.length > 0)
        regExp = new RegExp('(' + searchText + ')', 'i');

      return regExp;
    },

    /**
     * @param {string} rawQuery
     * @return {boolean} Whether this SearchRequest refers to an identical
     *     query.
     */
    isSame: function(rawQuery) {
      return this.rawQuery_ == rawQuery;
    },

    /**
     * Updates the result for this search request.
     * @param {boolean} found
     */
    updateMatches: function(found) {
      this.foundMatches_ = this.foundMatches_ || found;
    },

    /** @return {boolean} Whether any matches were found. */
    didFindMatches: function() {
      return this.foundMatches_;
    },
  };

  /** @interface */
  var SearchManager = function() {};

  SearchManager.prototype = {
    /**
     * @param {string} text The text to search for.
     * @param {!Node} page
     * @return {!Promise<!settings.SearchRequest>} A signal indicating that
     *     searching finished.
     */
    search: function(text, page) {}
  };

  /**
   * @constructor
   * @implements {SearchManager}
   */
  var SearchManagerImpl = function() {
    /** @private {?settings.SearchRequest} */
    this.activeRequest_ = null;

    /** @private {!TaskQueue} */
    this.queue_ = new TaskQueue();
    this.queue_.onEmpty(function() {
      this.activeRequest_.finished = true;
      this.activeRequest_.resolver.resolve(this.activeRequest_);
      this.activeRequest_ = null;
    }.bind(this));
  };
  cr.addSingletonGetter(SearchManagerImpl);

  SearchManagerImpl.prototype = {
    /** @override */
    search: function(text, page) {
      // Creating a new request only if the |text| changed.
      if (!this.activeRequest_ || !this.activeRequest_.isSame(text)) {
        // Resolving previous search request without marking it as
        // 'finished', if any, and dropping all pending tasks.
        this.queue_.reset();
        if (this.activeRequest_)
          this.activeRequest_.resolver.resolve(this.activeRequest_);

        this.activeRequest_ = new SearchRequest(text);
      }

      this.queue_.addTopLevelSearchTask(
          new TopLevelSearchTask(this.activeRequest_, page));

      return this.activeRequest_.resolver.promise;
    },
  };

  /** @return {!SearchManager} */
  function getSearchManager() {
    return SearchManagerImpl.getInstance();
  }

  /**
   * Sets the SearchManager singleton instance, useful for testing.
   * @param {!SearchManager} searchManager
   */
  function setSearchManagerForTesting(searchManager) {
    SearchManagerImpl.instance_ = searchManager;
  }

  return {
    getSearchManager: getSearchManager,
    setSearchManagerForTesting: setSearchManagerForTesting,
    SearchRequest: SearchRequest,
  };
});
