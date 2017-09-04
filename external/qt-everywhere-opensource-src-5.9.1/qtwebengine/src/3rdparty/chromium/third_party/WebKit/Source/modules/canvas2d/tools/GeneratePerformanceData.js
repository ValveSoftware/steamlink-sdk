// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ==========//
// Constants //
// ==========//

var CALL_TYPE = {
  PATH : "PATH",
  IMAGE : "IMAGE",
  GET_DATA : "GET_DATA",
  PUT_DATA : "PUT_DATA"
}

var SHAPE_TYPE = {
  STROKE_PATH : "STROKE_PATH",
  FILL_CONVEX_PATH : "FILL_CONVEX_PATH",
  FILL_NON_CONVEX_PATH : "FILL_NON_CONVEX_PATH",
  STROKE_RECT : "STROKE_RECT",
  FILL_RECT : "FILL_RECT",
  STROKE_TEXT : "STROKE_TEXT",
  FILL_TEXT : "FILL_TEXT"
}

var SHAPE_STYLE = {
    COLOR : "COLOR",
    LINEAR_GRADIENT : "LINEAR_GRADIENT",
    RADIAL_GRADIENT: "RADIAL_GRADIENT",
    PATTERN : "PATTERN"
}

var IMAGE_TYPE = {
  DRAW_SVG_IMAGE : "DRAW_SVG_IMAGE",
  DRAW_PNG_IMAGE : "DRAW_PNG_IMAGE"
}

var NUMBER_OF_FRAME_PER_ESTIMATE = 3;
var NUMBER_OF_BLANK_FRAMES_BETWEEN_MEASUREMENTS = 5;

// The max frames per second (fps) should be less than 60 because the rendering
// speed should be the limiting factor, not screen refresh rate.
var MAX_FRAMES_PER_SECOND_FOR_MEASUREMENTS = 30;
var MIN_FRAMES_PER_SECOND_FOR_MEASUREMENTS = 10;
var TARGET_FPS = 20;
var TARGET_TPF = 1000 / TARGET_FPS;

// ================//
// State Variables //
// ================//

var recording_helper = new RecordingHelper();
recording_helper.restartRecordingTime();

// list of list of DrawParameter's to describe the each frame to draw
var list_of_frame_parameters = [];

 // list of PerformanceMeasurement
var performance_measurements = [];

var frame_index = 0;
var blank_counter = 0;

// =============================//
// Main rendering loop function //
// =============================//

function drawAndRecord(context) {
  if (frame_index >= list_of_frame_parameters.length) return;

  if (blank_counter > 0) {
    recording_helper.restartRecordingTime();
    blank_counter--;
    return;
  }

  var newMeasurement = false;
  if (blank_counter < 0) {
    if (frame_index == 0
        && recording_helper.getNumberOfFramesRecorded() == 0) {
      newMeasurement = true;
    } else {
      var tpf = recording_helper.getAverageTimePerFrame();
      var fps = recording_helper.getAverageFramesPerSecond();

      if (fps > MAX_FRAMES_PER_SECOND_FOR_MEASUREMENTS) {
        // too fast, increase quantity and try again
        var new_quantity = quantityForTarget(
          list_of_frame_parameters[frame_index][0].quantity,
          tpf, TARGET_TPF);

        list_of_frame_parameters[frame_index][0].quantity = new_quantity;

        // reset total bounding box area values
        for (var i = 0; i<list_of_frame_parameters[frame_index].length; i++) {
          list_of_frame_parameters[frame_index][i].resetBoundingBoxArea();
        }

        newMeasurement = true;

      } else if (fps < MIN_FRAMES_PER_SECOND_FOR_MEASUREMENTS) {
        // too slow, move on to the next measurement
        console.log("Too slow - moving on to next measurement.");
        frame_index++;
        newMeasurement = true;
      }

      if (recording_helper.getNumberOfFramesRecorded() >=
          NUMBER_OF_FRAME_PER_ESTIMATE && newMeasurement == false) {
        if (fps < MAX_FRAMES_PER_SECOND_FOR_MEASUREMENTS) {
          // Redering speed, not refresh rate is the limiting factor
          console.log("Recording: frame_index = " +
                      frame_index + ",\n\ttfp = "+ tpf +
                      "\n\tfps = " + fps);

          performance_measurements.push(
              new PerformanceMeasurement(
                tpf, fps,
                list_of_frame_parameters[frame_index]));

        }

        frame_index++;
        newMeasurement = true;
      }
    }

    if (newMeasurement) {
      recording_helper.restartRecordingTime();
      blank_counter = NUMBER_OF_BLANK_FRAMES_BETWEEN_MEASUREMENTS;
      if (frame_index >= list_of_frame_parameters.length) {
        console.log(performance_measurements);
        drawCSVString(getCSVString(performance_measurements));
        return;
      }
    }
  }

  if (blank_counter == 0) {
    recording_helper.restartRecordingTime();
    blank_counter--;
  }

  DrawParameters(context, list_of_frame_parameters[frame_index]);
  recording_helper.recordFrame();
}

// =====================//
// Data storage classes //
// =====================//

function DrawSize(width, height) {
    this.width = Math.round(width);
    this.height = Math.round(height);
}

function DrawParameter(
  call_type, draw_sub_type, size, shadow_blur, quantity, fill_style) {
    this.call_type = call_type; // path, image, put or get image data
    this.draw_sub_type = draw_sub_type; // image type or path type
    this.fill_style = fill_style; // for paths only
    this.shadow_blur = shadow_blur;

    // Target size of side of square bounding box. Actuall size can differ.
    // total_bounding_box_area is returned by draw call and represents actuall
    // area of the call.
    this.size = size;

    this.total_bounding_box_area = 0; // computed elsewhere
    this.total_shadow_bounding_box_area = 0; // computed elsewhere
    this.total_bounding_box_perimeter = 0; // computed elsewhere

    this.resetBoundingBoxArea = function() {
      this.total_bounding_box_area = 0;
      this.total_shadow_bounding_box_area = 0;
      this.total_bounding_box_perimeter = 0;
    }

    this.quantity = quantity;
}

function PerformanceMeasurement(tpf, fps, parameters) {
    this.tpf = tpf; // time per frame
    this.fps = fps; // frames per second
    this.parameters = parameters; // DrawParameter object
}

// ===========================================================//
// Utility functions for generating lists of frame parameters //
// ===========================================================//

function generatePathFrameParameters() {
  var path_sizes = [20, 50, 200, 300, 400];
  var quantities = [10, 20, 50, 100, 500, 1000, 2000];
  var shadow_blurs = [0, 1, 10, 20];

  // Only draw combinations that contain shadows with this probability
  // to reduce the number of permutations.
  var shadowedPermutationProbability = 0.1;

  var list_of_frame_parameters = [];

  for (shape_type in SHAPE_TYPE) {
    for (var size_i = 0; size_i < path_sizes.length; size_i++) {
      for (var qty_i = 0; qty_i < quantities.length; qty_i++) {
        for (var sh_i = 0; sh_i < shadow_blurs.length; sh_i++) {
          for (fill_style in SHAPE_STYLE) {
            if (shadow_blurs[sh_i] == 0
                || getTrueWithProbability(shadowedPermutationProbability)) {

              list_of_frame_parameters.push([
                new DrawParameter(CALL_TYPE.PATH, shape_type,
                                  path_sizes[size_i], shadow_blurs[sh_i],
                                  quantities[qty_i], fill_style)]);
            }
          }
        }
      }
    }
  }
  return list_of_frame_parameters;
}

function generateImageFrameParameters() {
  var png_image_sizes = [5, 10, 20, 50, 90];
  var svg_image_sizes = [5, 20, 50, 90, 200, 300];

  var png_quantities = [10, 50, 100, 500, 1000];
  var svg_quantities = [10, 50, 100, 200, 500];
  var shadow_blurs = [0, 1, 5, 10];

  // Only draw combinations that contain shadows with this probability
  // to reduce the number of permutations.
  var shadowedPermutationProbability = 0.1;

  var list_of_frame_parameters = [];

  for (var size_i = 0; size_i < png_image_sizes.length; size_i++) {
    for (var qty_i = 0; qty_i < png_quantities.length; qty_i++) {
      for (var sh_i = 0; sh_i < shadow_blurs.length; sh_i++) {
        if (shadow_blurs[sh_i] == 0
            || getTrueWithProbability(shadowedPermutationProbability)) {

          list_of_frame_parameters.push([
            new DrawParameter(CALL_TYPE.IMAGE,
                              IMAGE_TYPE.DRAW_PNG_IMAGE,
                              png_image_sizes[size_i],
                              shadow_blurs[sh_i],
                              png_quantities[qty_i], "")]);
        }
      }
    }
  }

  for (var size_i = 0; size_i < svg_image_sizes.length; size_i++) {
    for (var qty_i = 0; qty_i < svg_quantities.length; qty_i++) {
      for (var sh_i = 0; sh_i < shadow_blurs.length; sh_i++) {
        if (shadow_blurs[sh_i] == 0
            || getTrueWithProbability(shadowedPermutationProbability)) {

          list_of_frame_parameters.push([
            new DrawParameter(CALL_TYPE.IMAGE,
                              IMAGE_TYPE.DRAW_SVG_IMAGE,
                              svg_image_sizes[size_i],
                              shadow_blurs[sh_i],
                              svg_quantities[qty_i], "")]);
        }
      }
    }
  }
  return list_of_frame_parameters;
}

function generatePutDataFrameParameters(context) {
  var sizes = [20, 50, 100, 200, 300];
  var quantities = [10, 50, 100, 500, 1000, 2000];

  var list_of_frame_parameters = [];

  for (var size_i = 0; size_i < sizes.length; size_i++) {
    for (var qty_i = 0; qty_i < quantities.length; qty_i++) {
      // Call putImageData so it aquires and caches image data of
      // the correct dimensions
      putImageData(context, sizes[size_i]);

      list_of_frame_parameters.push([
        new DrawParameter(CALL_TYPE.PUT_DATA,
                          "",
                          sizes[size_i],
                          0,
                          quantities[qty_i], "")]);
    }
  }

  return list_of_frame_parameters;
}

function generateGetDataFrameParameters(context) {
  var sizes = [20, 50, 100, 200, 300];
  var quantities = [10, 50, 100, 500, 1000, 2000];

  var list_of_frame_parameters = [];

  for (var size_i = 0; size_i < sizes.length; size_i++) {
    for (var qty_i = 0; qty_i < quantities.length; qty_i++) {
      list_of_frame_parameters.push([
        new DrawParameter(CALL_TYPE.GET_DATA,
                          "",
                          sizes[size_i],
                          0,
                          quantities[qty_i], "")]);
    }
  }

  return list_of_frame_parameters;
}

// Draw each DrawParameter in the parameter_list on the canvas context,
// updates their total_bounding_box_area, total_bounding_box_perimeter
// and total_shadow_bounding_box_area fields.
function DrawParameters(context, parameter_list) {
  for (var i = 0; i < parameter_list.length; i++) {
    var p = parameter_list[i];
    p.resetBoundingBoxArea();
    for (var count = 0; count < p.quantity; count++) {
      var draw_size;
      switch (p.call_type) {
        case CALL_TYPE.PATH:
          setShapeStyle(context, p.fill_style);
          setShadeStyle(context, p.shadow_blur);
          draw_size = drawShape(context, p.draw_sub_type, p.size);
        break;
        case CALL_TYPE.IMAGE:
          setShadeStyle(context, p.shadow_blur);
          draw_size = drawImageType(context, p.draw_sub_type, p.size);
        break;
        case CALL_TYPE.GET_DATA:
          draw_size = getImageData(context, p.size);
        break;
        case CALL_TYPE.PUT_DATA:
          draw_size = putImageData(context, p.size);
        break;
        default:
          draw_size = new DrawSize(0, 0);
      }

      p.total_bounding_box_area +=
        draw_size.width * draw_size.height;

      p.total_bounding_box_perimeter +=
        (2*draw_size.width) + (2*draw_size.height);

      if (p.call_type == CALL_TYPE.PATH || p.call_type == CALL_TYPE.IMAGE) {
        p.total_shadow_bounding_box_area +=
          (draw_size.width+2*p.shadow_blur) *
          (draw_size.height+2*p.shadow_blur);
      }
    }
  }
}

// =======================================================================//
// Utility functions to make a CSV string that encodes the generated data //
// =======================================================================//

function getDataFrame(performance_measurements) {
  var data = {};

  // shadow_blur * total bounding box area
  data["SHADOW_BLUR_TIMES_AREA"] = [];

  // shadow_blur^2 * total bounding box area
  data["SHADOW_BLUR_SQUARED_TIMES_AREA"] = [];

  // shadow_blur^2 * total shadow bounding box area
  data["SHADOW_BLUR_SQUARED_TIMES_SHADOW_AREA"] = [];

  // the number of draw calls that have a shadow
  data["NUM_SHADOWS"] = [];

  data["CALL_TYPE"] = [];

  for (var i = 0; i < performance_measurements.length; i++) {
    var performance_measurement = performance_measurements[i];
    for (var j = 0; j < performance_measurement.parameters.length; j++) {
      var p = performance_measurement.parameters[j];
      var call_type = p.call_type + "_" + p.draw_sub_type;
      var call_type_area = p.call_type + "_" + p.draw_sub_type + "_AREA";

      data[call_type] = [];
      data[call_type_area] = [];

      if (p.call_type == CALL_TYPE.PATH) {
        data[p.fill_style] = [];
        data[p.fill_style + "_AREA"] = [];
      }
    }
  }

  for (var i = 0; i < performance_measurements.length; i++) {
    var performance_measurement = performance_measurements[i];
    for (var j = 0; j < performance_measurement.parameters.length; j++) {
      var p = performance_measurement.parameters[j];
      for (var key in data) {
          data[key].push(0);
      }

      var call_type = p.call_type + "_" + p.draw_sub_type;
      var call_type_area = p.call_type + "_" + p.draw_sub_type + "_AREA";

      data[call_type][i] += p.quantity;

      data[call_type_area][i] += p.total_bounding_box_area;

      if (p.call_type == CALL_TYPE.PATH) {
        data[p.fill_style][i] += p.quantity;
        data[p.fill_style + "_AREA"][i] += p.total_bounding_box_area;
      }

      if (p.call_type == CALL_TYPE.PATH || p.call_type == CALL_TYPE.IMAGE) {
        data["NUM_SHADOWS"][i] += (p.shadow_blur > 0.0) ? p.quantity : 0;

        data["SHADOW_BLUR_TIMES_AREA"][i] +=
          p.total_bounding_box_area * p.shadow_blur;

        data["SHADOW_BLUR_SQUARED_TIMES_AREA"][i] +=
          p.total_bounding_box_area * p.shadow_blur * p.shadow_blur;

        data["SHADOW_BLUR_SQUARED_TIMES_SHADOW_AREA"][i] +=
          p.total_shadow_bounding_box_area * p.shadow_blur * p.shadow_blur;
      }

      data["CALL_TYPE"][i] = p.call_type;
    }
  }

  return data;
}

function getCSVString(performance_measurements) {
  var csv_string = "";

  var data_frame = getDataFrame(performance_measurements);

  csv_string += "TIME_PER_FRAME, FRAMES_PER_SECOND"
  for (var key in data_frame) {
      csv_string += ", " + key;
  }
  csv_string += "\n";

  for (var i = 0; i < performance_measurements.length; i++) {
      var performance_measurement = performance_measurements[i];
      csv_string += performance_measurement.tpf;
      csv_string += ", " + performance_measurement.fps;

      for (var key in data_frame) {
          csv_string +=  ", " + data_frame[key][i];
      }
      csv_string += "\n";
  }

  return csv_string;
}

function drawCSVString(csv_string) {
  var div = document.getElementById("csv_string");
  div.innerHTML = csv_string;
}

// =========================================================================//
// Utility functions to set canvas parameters and perform canvas draw calls //
// =========================================================================//

function drawShape(context, shape_type, size) {
  context.lineWidth = 3;
  switch (shape_type) {
    case SHAPE_TYPE.STROKE_PATH:
      var radius = size/2;
      context.beginPath();
      context.ellipse(400, 400, radius, radius, 0, 0, 2*Math.PI, false);
      context.stroke();
      return new DrawSize(size, size);

    case SHAPE_TYPE.FILL_CONVEX_PATH:
      var radius = size/2;
      context.beginPath();
      context.ellipse(400, 400, radius, radius, 0, 0, 2*Math.PI, false);
      context.fill();
      return new DrawSize(size, size);

    case SHAPE_TYPE.FILL_NON_CONVEX_PATH:
      var radius = size/2;
      context.beginPath();
      context.lineTo(400, 400);
      context.arc(400, 400, radius, 0, Math.PI*3/2, false);
      context.lineTo(this.x, this.y);
      context.fill();
      return new DrawSize(size, size);

    case SHAPE_TYPE.STROKE_RECT:
      context.beginPath();
      context.rect(100, 100, size, size);
      context.stroke();
      return new DrawSize(size, size);

    case SHAPE_TYPE.FILL_RECT:
      context.beginPath();
      context.rect(100, 100, size, size);
      context.fill();
      return new DrawSize(size, size);

    case SHAPE_TYPE.STROKE_TEXT:
      context.font = size + "px Georgia";
      context.strokeText("M", 40, 500);
      return new DrawSize(context.measureText("M").width, size);

    case SHAPE_TYPE.FILL_TEXT:
      context.font = size + "px Georgia";
      context.fillText("M", 40, 500);
      return new DrawSize(context.measureText("M").width, size);

    default:
      console.log("Invalid SHAPE_TYPE: " + shape_type);
      return new DrawSize(0, 0);
  }
}

var svg_image = document.getElementById("svg_image_element");
var png_image = document.getElementById("png_image_element");
function drawImageType(context, image_type, size) {
  switch (image_type) {
    case IMAGE_TYPE.DRAW_SVG_IMAGE:
      context.drawImage(svg_image, 50, 50, size, size);
    return new DrawSize(size, size);

    case IMAGE_TYPE.DRAW_PNG_IMAGE:
      if (size > png_image.width) {
        throw "In drawImageType (png) - size too big ";
      }
      if (size > png_image.height) {
        throw "In drawImageType (png) - size too big ";
      }
      context.drawImage(png_image, 0, 0, size, size, 50, 50, size, size);
    return new DrawSize(size, size);
  }
}

function setShapeStyle(context, shape_style, area) {
  var gradient = "";
  switch (shape_style) {
    case SHAPE_STYLE.LINEAR_GRADIENT:
      var gradient = context.createLinearGradient(0, 0, 800, 600);
      gradient.addColorStop(0, "blue");
      gradient.addColorStop(1, "white");
      context.fillStyle = gradient;
      context.strokeStyle = gradient;
    break;
    case SHAPE_STYLE.COLOR:
      context.fillStyle = "blue";
      context.strokeStyle = "blue";
    break;
    case SHAPE_STYLE.RADIAL_GRADIENT:
      var gradient =
        context.createRadialGradient(400, 300, 100, 400, 300, 800);
      gradient.addColorStop(0, "blue");
      gradient.addColorStop(1, "white");
      context.fillStyle = gradient;
      context.strokeStyle = gradient;
    break;
    case SHAPE_STYLE.PATTERN:
      var pattern = context.createPattern(png_image,"repeat");
      context.fillStyle = pattern;
      context.strokeStyle = pattern;
    break;
    default:
      console.log("Invalid SHAPE_STYLE: " + shape_style);
    break;
  }
}

function setShadeStyle(context, shadow_blur) {
  context.shadowBlur = shadow_blur;
  context.shadowColor= "black";
}

function getImageData(context, size) {
  var image = context.getImageData(10,10, size, size);
  return new DrawSize(size, size);
}


var image_data_cache = {};
function putImageData(context, size) {
  if (!(size in image_data_cache)) {
    // Cache a data source so that subsequent calls with the same size don't
    // need to generate a new data source of the appropriate size
    image_data_cache[size] = context.createImageData(size, size);
  }

  context.putImageData(image_data_cache[size], 10, 10);
  return new DrawSize(size, size);
}

// ================//
// Other utilities //
// ================//

function RecordingHelper() {
  this.num_frames_since_restart = 0;
  this.recording_start_time = Date.now();

  this.restartRecordingTime = function() {
    this.num_frames_since_restart = 0;
    this.recording_start_time = Date.now();
  }

  this.recordFrame = function() {
    this.num_frames_since_restart += 1;
  }

  this.getAverageTimePerFrame = function() {
    var time_interval = Date.now() - this.recording_start_time;
    return time_interval / this.num_frames_since_restart;
  }

  this.getAverageFramesPerSecond = function() {
    return 1000 / this.getAverageTimePerFrame();
  }

  this.getNumberOfFramesRecorded =  function() {
    return this.num_frames_since_restart;
  }
}

function getTrueWithProbability(probability) {
  return Math.random() < probability;
}

// Given the current quantity of a call, the current time per frame and target
// time per frame, return a suggested quantity that should yield a rendering
// speed closer to the target time per frame assuming that the time per frame
// is linearly proportional to the quantity.
function quantityForTarget(current_quantity, current_tpf, target_tpf) {
  if (current_quantity <= 0) return 1;
  return Math.round(current_quantity * target_tpf / current_tpf);
}

