// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

exports.didCreateDocumentElement = function() {
  var root = document.documentElement.createShadowRoot();
  root.appendChild(document.createElement('style')).innerText =
      // TODO(jeremya): switch this to use automatic inlining once grit
      // supports inlining into JS. See http://crbug.com/146319.
      "x-titlebar { height: 24px; width: 100%; " +
        "position: fixed; left: 0; top: 0; }\n" +
      "div { margin-top: 24px; position: absolute; top: 0px; width: 100%; " +
        "-webkit-widget-region: region(control rectangle); }\n" +
      ":-webkit-full-screen * { display: none; }\n" +
      ":-webkit-full-screen-document * { display: none; }\n" +
      "div:-webkit-full-screen, div:-webkit-full-screen-document { " +
        "margin-top: 0; }\n" +
      "button { -webkit-widget-region: region(control rectangle); }\n" +
      "button.close { border: 0; background-color: transparent; " +
      "width: 16px; height: 16px; " +
      "position: absolute; right: 4px; top: 4px; }\n" +
      "button.close { background-image: url(data:image/png;base64," +
      "iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAA9ElEQVQ4T7VTQQ6CMBCk0H" +
      "AyIfAQbiZ+QHyDL/QLxqvx4MWDB+MvFAWMAuKsacmmSjkQSDbQ2Z3Z3WkQzsBHDOQ7owgs" +
      "MdUacTGmi3BeIFYcNycgciGlfFRVtcd3qoojz/PmdV0XOD8RGy1iCoQgT5G8IyREjni7IC" +
      "cg58ilwA7A8i4BwgMUxkKIV9M0PggTAoFlJpnwLhO5iEuFapq2s20CyoWIGbpeaRICyrI8" +
      "89FtAtqwGxdQ65yYsV8NcwVN5obR/uTJW4mQsfp2fgToGjPqbBjWeoJVfNRsbSskSO7+7B" +
      "sAiznZdgu6Qe97lH+htysv+AA10msRAt5JYQAAAABJRU5ErkJggg==); }\n" +
      "button.close:hover { background-image: url(data:image/png;base64," +
      "iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAABTElEQVQ4T2NkoBAwUqifAc" +
      "WA////KwANFAPiV4yMjA+QDcclBzcApCA6Otpz2bJluQkJCf3z58/fDTMEnxyyAWZADQuA" +
      "tj4B4ncpKSnbZs+efQjkCqjBmUDmMyD+ADSwD6j2FEgOxQWJiYmuCxYscIYawpWamnr89+" +
      "/fHECxbKjmB2VlZbs6OzsvwFyHEQZATXZz5syxAGr4BMR8QCwJDYvn1dXVO1taWi4ihw9G" +
      "LID8m5aWZgt0viXUEBaQAUDNh9E1o3gBFuIgA6Be8QKK3QXiLyA5oNMvIDsdph7DC9AASw" +
      "cquI9sAJDNk5GRcX769OlHsXoBKapAoQ2KiQcgPwMDkbGrq8sGyP8DChNQwM6aNeswRiAC" +
      "DYBF4yOgwnuwAAM5NTMz03rGjBnWsIAFql2ANxqB/l2B7F/kgCUYjUBbyEvKsFAllaY4Nw" +
      "IAmJDPEd+LFvYAAAAASUVORK5CYII=); }\n" +
      "button.close:active { background-image: url(data:image/png;base64," +
      "iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAAZ0lEQVQ4T2NkoBAwUqifge" +
      "oG2AFd1AfERUB8CM11WOXQXXAGSROyITDNMGkTGAPdAHSFIENAAOQqGEBxHbYwQDcE2ScY" +
      "XsMViNgMwRYuOGOBIgMo8gLFgUi1aCQ7IZGcNaieF0h2AQCMABwRdsuhtQAAAABJRU5Erk" +
      "Jggg==); }\n"
  var titlebar = root.appendChild(document.createElement('x-titlebar'));
  var closeButton = titlebar.appendChild(document.createElement('button'));
  closeButton.className = 'close'
  closeButton.addEventListener('click', function() { window.close(); });
  var container = root.appendChild(document.createElement('div'));
  container.appendChild(document.createElement('content'));
}
