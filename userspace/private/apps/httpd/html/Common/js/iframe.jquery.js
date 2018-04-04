/*
 * IFrame Loader Plugin for JQuery
 * - Notifies your event handler when iframe has finished loading
 * - Your event handler receives loading duration (as well as iframe)
 * - Optionally calls your timeout handler
 *
 * http://project.ajaxpatterns.org/jquery-iframe
 *
 * The MIT License
 *
 * Copyright (c) 2009, Michael Mahemoff
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

(function($) {
  var timer;
  $.fn.src = function(url, onLoad, options) {
    var defaults = {
      timeout: 0,
      onTimeout: null,
      onReady: null
    }
    var iframe = $(this);
    iframe.unbind("load");
    if (timer) clearTimeout(timer);
    var opts = $.extend(defaults, options);
    opts.frameactive = true;
    var startTime = (new Date()).getTime();
    if (opts.timeout) {
      var timer = setTimeout(function() {
        opts.frameactive=false; 
        iframe.load(null);
        if (opts.onTimeout) opts.onTimeout(iframe.get(0), opts.timeout);
      }, opts.timeout);
    }
    if (opts.onReady) { 
      iframe.unbind("ready");
      iframe.ready(function() {
        if (opts.frameactive) {
          var duration=(new Date()).getTime()-startTime;
          opts.onReady(this, duration);
        }
      });
    }

    var onloadHandler = function() {
      var duration=(new Date()).getTime()-startTime;
      if (timer) clearTimeout(timer);
      if (onLoad && opts.frameactive) onLoad(this, duration);
      opts.frameactive=false;
    }
    iframe.attr("src", url);
    iframe.get()[0].onload = onloadHandler;
    opts.completeReadyStateChanges=0;
    iframe.get()[0].onreadystatechange = function() { // IE ftw
	    if (++(opts.completeReadyStateChanges)==3) onloadHandler();
    }

  }
})(jQuery);
