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

var net = net || {};

(function () {

net.get = function(url)
{
    return net.ajax({
        url: url
    });
};

net.json = function(url)
{
    return net.ajax({
        url: url,
        parse: function(xhr) {
            return JSON.parse(xhr.responseText);
        }
    });
};

net.xml = function(url)
{
    return net.ajax({
        url: url,
        parse: function(xhr) {
            return xhr.responseXML;
        }
    });
};

net.ajax = function(options)
{
    return new Promise(function(resolve, reject) {
        var xhr = new XMLHttpRequest();
        var method = options.type || 'GET';
        var async = true;
        xhr.open(method, options.url, async);
        xhr.onload = function() {
            if (xhr.status == 200) {
                if (options.parse)
                    resolve(options.parse(xhr));
                else
                    resolve(xhr.responseText);
            }
            else if (xhr.status != 200)
                reject(Error(xhr.statusText));
        };
        xhr.onerror = function(error) {
            reject(error);
        };
        var data = options.data || null;
        if (data)
            xhr.setRequestHeader("content-type","application/x-www-form-urlencoded");
        xhr.send(data);
    });
};

net.post = function(url, data)
{
    return net.ajax({
        url: url,
        type: 'POST',
        data: data,
    });

};

net.probe = function(url)
{
    return net.ajax({
        url: url,
        type: 'HEAD',
    });
};

// We use XMLHttpRequest and CORS to fetch JSONP rather than using script tags.
// That's better for security and performance, but we need the server to cooperate
// by setting CORS headers.
net.jsonp = function(url)
{
    return net.ajax({
        url: url,
    }).then(function(jsonp) {
        return base.parseJSONP(jsonp);
    }).catch(function(error) {
        return {};
    });
};

})();
