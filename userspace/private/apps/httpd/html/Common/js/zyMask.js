/*
 * zyMask.js
 *
 * Copyright (c) 2010
 * Joze Chang @zyxel.com.tw
 *
 * Dual licensed under the GPL (http://www.gnu.org/licenses/gpl.html)
 * and MIT (http://www.opensource.org/licenses/mit-license.php) licenses.
 *
 * $Date: 2010-02-04 $
 * depend on simpleModal(http://www.ericmmartin.com/projects/simplemodal/)
 * $Rev: 002 $
 *
 */
;
(function($){

    $.extend({
        zyMask: function(content, contentSize, settings){
        
            //check whether simpleModal is loaded or not.
            if ($.modal == null) {
                alert('depend on simpleModal plugin');
                return;
            }
            
            var modifyPosition = function(){
                var modalContainer = $('#modalContainer');
                if (modalContainer.length != 0) {
                    var leftS = $(window).width() / 2 - contentSize[0] / 2;
                    var topS = $(window).height() / 2 - contentSize[1] / 2;
                    modalContainer.css('margin-left', leftS).css('margin-top', topS);
                }
                else {
                    $(window).unbind('resize', modifyPosition);
                }
            }
            var _settings;
            var _defaultSettings = {
                position: ['0', '0'],
                opacity: 60,
                overlayCss: {
                    backgroundColor: "#000"
                },
                containerCss: {
                    margin: "0",
                    background: "none",
                    color: "#FFF",
                    width: contentSize[0] + "px",
                    height: contentSize[1] + "px"
                },
                containerId: 'modalContainer',
                onload: function(){},
                onShow: function(e ){
                    modifyPosition();
                    $(window).resize(modifyPosition);
                    _settings.onload(e);

                }
              , onClose: function(dialog){
                    dialog.data.fadeOut(0, function(){
                        dialog.container.hide(0);
                        dialog.overlay.fadeOut(100, function(){
                            $.modal.close();
                        });
                    });
                }
            }
            
            _settings = $.extend(_defaultSettings, settings);
            var modalObj =  $.modal(content, _settings);
            return modalObj;
        },
        
        closezyMask: function(){
            $.modal.close();
        }
    });
    
    
})(jQuery);


