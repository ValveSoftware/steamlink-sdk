// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define('stash_client', [
    'async_waiter',
    'content/public/renderer/frame_service_registry',
    'extensions/common/mojo/stash.mojom',
    'mojo/public/js/buffer',
    'mojo/public/js/codec',
    'mojo/public/js/core',
    'mojo/public/js/router',
], function(asyncWaiter, serviceProvider, stashMojom, bufferModule,
            codec, core, routerModule) {
  /**
   * @module stash_client
   */

  var service = new stashMojom.StashService.proxyClass(new routerModule.Router(
      serviceProvider.connectToService(stashMojom.StashService.name)));

  /**
   * A callback invoked to obtain objects to stash from a particular client.
   * @callback module:stash_client.StashCallback
   * @return {!Promise<!Array<!Object>>|!Array<!Object>} An array of objects to
   *     stash or a promise that will resolve to an array of objects to stash.
   *     The exact type of each object should match the type passed alongside
   *     this callback.
   */

  /**
   * A stash client registration.
   * @constructor
   * @private
   * @alias module:stash_client~Registration
   */
  function Registration(id, type, callback) {
    /**
     * The client id.
     * @type {string}
     * @private
     */
    this.id_ = id;

    /**
     * The type of the objects to be stashed.
     * @type {!Object}
     * @private
     */
    this.type_ = type;

    /**
     * The callback to invoke to obtain the objects to stash.
     * @type {module:stash_client.StashCallback}
     * @private
     */
    this.callback_ = callback;
  }

  /**
   * Serializes and returns this client's stashable objects.
   * @return
   * {!Promise<!Array<module:extensions/common/stash.mojom.StashedObject>>} The
   * serialized stashed objects.
   */
  Registration.prototype.serialize = function() {
    return Promise.resolve(this.callback_()).then($Function.bind(
        function(stashedObjects) {
      if (!stashedObjects)
        return [];
      return $Array.map(stashedObjects, function(stashed) {
        var builder = new codec.MessageBuilder(
            0, codec.align(this.type_.encodedSize));
        builder.encodeStruct(this.type_, stashed.serialization);
        var encoded = builder.finish();
        return new stashMojom.StashedObject({
          id: this.id_,
          data: new Uint8Array(encoded.buffer.arrayBuffer),
          stashed_handles: encoded.handles,
          monitor_handles: stashed.monitorHandles,
        });
      }, this);
    }, this)).catch(function(e) { return []; });
  };

  /**
   * The registered stash clients.
   * @type {!Array<!Registration>}
   */
  var clients = [];

  /**
   * Registers a client to provide objects to stash during shut-down.
   *
   * @param {string} id The id of the client. This can be passed to retrieve to
   *     retrieve the stashed objects.
   * @param {!Object} type The type of the objects that callback will return.
   * @param {module:stash_client.StashCallback} callback The callback that
   *     returns objects to stash.
   * @alias module:stash_client.registerClient
   */
  function registerClient(id, type, callback) {
    clients.push(new Registration(id, type, callback));
  }

  var retrievedStash = service.retrieveStash().then(function(result) {
    if (!result || !result.stash)
      return {};
    var stashById = {};
    $Array.forEach(result.stash, function(stashed) {
      if (!stashById[stashed.id])
        stashById[stashed.id] = [];
      stashById[stashed.id].push(stashed);
    });
    return stashById;
  }, function() {
    // If the stash is not available, act as if the stash was empty.
    return {};
  });

  /**
   * Retrieves the objects that were stashed with the given |id|, deserializing
   * them into structs with type |type|.
   *
   * @param {string} id The id of the client. This should be unique to this
   *     client and should be passed as the id to registerClient().
   * @param {!Object} type The mojo struct type that was serialized into the
   *     each stashed object.
   * @return {!Promise<!Array<!Object>>} The stashed objects. The exact type of
   *     each object is that of the |type| parameter.
   * @alias module:stash_client.retrieve
   */
  function retrieve(id, type) {
    return retrievedStash.then(function(stash) {
      var stashedObjects = stash[id];
      if (!stashedObjects)
        return Promise.resolve([]);

      return Promise.all($Array.map(stashedObjects, function(stashed) {
        var encodedData = new ArrayBuffer(stashed.data.length);
        new Uint8Array(encodedData).set(stashed.data);
        var reader = new codec.MessageReader(new codec.Message(
            new bufferModule.Buffer(encodedData), stashed.stashed_handles));
        var decoded = reader.decodeStruct(type);
        return decoded;
      }));
    });
  }

  function saveStashForTesting() {
    Promise.all($Array.map(clients, function(client) {
      return client.serialize();
    })).then(function(stashedObjects) {
      var flattenedObjectsToStash = [];
      $Array.forEach(stashedObjects, function(stashedObjects) {
        flattenedObjectsToStash =
            $Array.concat(flattenedObjectsToStash, stashedObjects);
      });
      service.addToStash(flattenedObjectsToStash);
    });
  }

  return {
    registerClient: registerClient,
    retrieve: retrieve,
    saveStashForTesting: saveStashForTesting,
  };
});
