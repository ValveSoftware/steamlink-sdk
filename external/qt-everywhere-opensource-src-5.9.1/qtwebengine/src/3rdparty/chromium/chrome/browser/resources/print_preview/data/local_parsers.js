// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /** Namespace that contains a method to parse local print destinations. */
  function LocalDestinationParser() {};

  /**
   * Parses a local print destination.
   * @param {!Object} destinationInfo Information describing a local print
   *     destination.
   * @return {!print_preview.Destination} Parsed local print destination.
   */
  LocalDestinationParser.parse = function(destinationInfo) {
    var options = {'description': destinationInfo.printerDescription};
    if (destinationInfo.printerOptions) {
      // Convert options into cloud print tags format.
      options.tags = Object.keys(destinationInfo.printerOptions).map(
          function(key) {return '__cp__' + key + '=' + this[key];},
          destinationInfo.printerOptions);
    }
    return new print_preview.Destination(
        destinationInfo.deviceName,
        print_preview.Destination.Type.LOCAL,
        print_preview.Destination.Origin.LOCAL,
        destinationInfo.printerName,
        false /*isRecent*/,
        print_preview.Destination.ConnectionStatus.ONLINE,
        options);
  };

  function PrivetDestinationParser() {}

  /**
   * Parses a privet destination as one or more local printers.
   * @param {!Object} destinationInfo Object that describes a privet printer.
   * @return {!Array<!print_preview.Destination>} Parsed destination info.
   */
  PrivetDestinationParser.parse = function(destinationInfo) {
    var returnedPrinters = [];

    if (destinationInfo.hasLocalPrinting) {
       returnedPrinters.push(new print_preview.Destination(
           destinationInfo.serviceName,
           print_preview.Destination.Type.LOCAL,
           print_preview.Destination.Origin.PRIVET,
           destinationInfo.name,
           false /*isRecent*/,
           print_preview.Destination.ConnectionStatus.ONLINE,
           { cloudID: destinationInfo.cloudID }));
    }

    if (destinationInfo.isUnregistered) {
      returnedPrinters.push(new print_preview.Destination(
          destinationInfo.serviceName,
          print_preview.Destination.Type.GOOGLE,
          print_preview.Destination.Origin.PRIVET,
          destinationInfo.name,
          false /*isRecent*/,
          print_preview.Destination.ConnectionStatus.UNREGISTERED));
    }

    return returnedPrinters;
  };

  function ExtensionDestinationParser() {}

  /**
   * Parses an extension destination from an extension supplied printer
   * description.
   * @param {!Object} destinationInfo Object describing an extension printer.
   * @return {!print_preview.Destination} Parsed destination.
   */
  ExtensionDestinationParser.parse = function(destinationInfo) {
    var provisionalType =
        destinationInfo.provisional ?
            print_preview.Destination.ProvisionalType.NEEDS_USB_PERMISSION :
            print_preview.Destination.ProvisionalType.NONE;

    return new print_preview.Destination(
        destinationInfo.id,
        print_preview.Destination.Type.LOCAL,
        print_preview.Destination.Origin.EXTENSION,
        destinationInfo.name,
        false /* isRecent */,
        print_preview.Destination.ConnectionStatus.ONLINE,
        {description: destinationInfo.description || '',
         extensionId: destinationInfo.extensionId,
         extensionName: destinationInfo.extensionName || '',
         provisionalType: provisionalType});
  };

  // Export
  return {
    LocalDestinationParser: LocalDestinationParser,
    PrivetDestinationParser: PrivetDestinationParser,
    ExtensionDestinationParser: ExtensionDestinationParser
  };
});
