// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function pathIsVideoFile(path) {
  return /\.(3gp|asf|avi|di?vx|f4v|fbr|mov|mp4|m4v|mpe?g4?|ogm|ogv|ogx|webm|wmv?|xvid)$/i.test(path);
}

function pathIsAudioFile(path) {
  return /\.(aac|aiff|atrac|cda|flac|m4a|mp3|pcm|oga|ogg|raw|wav)$/i.test(path);
}

function pathIsImageFile(path) {
  return /\.(bmp|gif|jpe?g|ico|png|webp)$/i.test(path);
}

function pathIsHtmlFile(path) {
  return /\.(htm|html|txt)$/i.test(path);
}

function pathIsPdfFile(path) {
  return /\.(pdf)$/i.test(path);
}
