/*
 * zyCheckTree.js
 *
 * Copyright (c) 2010
 * zyxe.com
 *
 * licensed under the GPL (http://www.gnu.org/licenses/gpl.html)
 *
 */
(function($){

    $.fn.zyCheckTree = function(settings){
    
        var _defaultSetting = {
            parseJson:'menu.json',
            isParseTreeByJson: true,
            isCollapsble: true,
            loseControlOptions:[],//loseControlOptions:['0', '2_1'], 
            canLabelCheck: true,
            root: null,
            parseFn: function(i, item){return true;} //true:pass
        }
        
        var _settings = $.extend(_defaultSetting, settings);
        
        this.clearAll = function(){
            this.each(function(){
                 $('.checkbox:not(.loseControl)', this).removeClass('checked').removeClass('half_checked');
            });
        }
        
        this.selectAll = function(){
            this.each(function(){
                $('.checkbox:not(.loseControl)', this).removeClass('half_checked').addClass('checked');
                //for lose control
                $('li.loseControl', this).parent('.tree').each(function(){
                    parentStatusUpdate($(this));
                });
            });
        }

        this.setCheckedItems = function(checkArray)
        {
            var checkItems = checkArray.split(",");
            this.each(function(){
                 $('.checkbox', this).removeClass('checked').removeClass('half_checked');
                  for (var j = 0; j < checkItems.length; j++) {
                  
                      var checkNode =$(this).find('[treeId=node_'+checkItems[j]+']');
                      nodeStatusUpdate(checkNode);
                  }
            })
        }
 
        this.getCheckedItems = function()
        {
            var retArray =new Array();
        
            this.each(function(){        
                var checkedArray = new Array();
                $('.checkbox.checked', this).each(function(){
                   var parentNode;
                        $(this).parents('ul.tree').each(function(i, item){
                            if (i == 0) 
                                parentNode = $(item);
                        })

                   if (parentNode.siblings('.checkbox').length==0 || parentNode.siblings('.checkbox').hasClass('half_checked')) 
                            checkedArray.push($(this).attr('treeId').split('node_')[1]);
                });
                 retArray.push(checkedArray);
            })
            return retArray.length==1?retArray[0]:retArray;
        }
        
        this.setCheckedJsonIDs = function(checkArray)
        {
            if(this.isParseTreeByJson==false)
            return;

            this.each(function(){
                 $('.checkbox', this).removeClass('checked').removeClass('half_checked');
                  for (var j = 0; j < checkArray.length; j++) {
                      var checkNode = $(".checkbox[targetID='"+checkArray[j]+ "']", this)
                      nodeStatusUpdate(checkNode);
                  }
            })
        }
        
        this.getCheckedJsonIDs = function()
        {
            if(this.isParseTreeByJson==false)
            return null;
            
            var retArray =new Array();
        
            this.each(function(){        
                var checkedArray = new Array();
                $('.checkbox.checked', this).each(function(){
                   var parentNode;
                        $(this).parents('ul.tree').each(function(i, item){
                            if (i == 0) 
                                parentNode = $(item);
                        })

                   if (parentNode.siblings('.checkbox').length!=0 || parentNode.siblings('.checkbox').hasClass('half_checked')) {
                            var nodeName = $(this).attr('targetID').split("-");
                            checkedArray.push(nodeName[1]);
                   }
                });
                retArray.push(checkedArray);
            })
            return retArray.length==1?retArray[0]:retArray; 
        }

        function append2Root(childObj, rootText){
              var contenter = $('<ul class="tree"></ul>');
              var liNode = $('<li></li>');
              var inputNode = $('<div class="checkbox" treeId="node_root" targetId="root"></div>');
              var labelNode = $(' <LABEL>' +  rootText + '</LABEL>');
              
              
              liNode.append(inputNode);
              liNode.append(labelNode);
              liNode.append(childObj);
              contenter.append(liNode);
              return contenter;
        }
        
        function createNode(data, recursiveCounter, targetIDStr){
            if (recursiveCounter == undefined) 
                recursiveCounter = "";

            var contenter = $('<ul class="tree"></ul>');
            var counter = 0;
            $.each(data, function(i, item){
                
                if (!_settings.parseFn(i, item))
                return;
                
                if (item.title == undefined) 
                return;
                var liNode = $('<li></li>');
                
                var nodeStr = recursiveCounter + '_' + counter;
                counter++;
                
                var IDStr;
                if (targetIDStr == undefined) 
                IDStr = i; 
                else
                IDStr = targetIDStr + '-' + i;
                var inputNode = $('<div class="checkbox" treeId="node' + nodeStr + '" targetID='+IDStr+'  ></div>');
                liNode.append(inputNode);
                
                var labelNode = $(' <LABEL>' + item.title + '</LABEL>');
                liNode.append(labelNode);
                
                if (item.submenu != undefined) {
                    var subMenuContenter = createNode(item.submenu, nodeStr, IDStr);
                    liNode.append(subMenuContenter);
                }
                contenter.append(liNode);
            });
            
           if(_settings.root!=null)
           contenter = append2Root(contenter, _settings.root);
            
            return contenter;
        }

        function parentStatusUpdate(treeObj){
            if (treeObj.length > 1) {
                var parentNode;
                treeObj.each(function(i, item){
                    if (i == 0) {
                        parentNode = item;
                    }
                })
                treeObj = $(parentNode);
            }
            
            var childrenNum = treeObj.contents().find('.checkbox').length;
            var checkedNum = treeObj.contents().find('.checkbox.checked').length;

            if (childrenNum == checkedNum) {
                treeObj.siblings('.checkbox').removeClass('half_checked').addClass('checked');
            }
            else 
                if (childrenNum > checkedNum && checkedNum != 0) {
                    treeObj.siblings('.checkbox').removeClass('checked').addClass('half_checked');
                }
                else 
                    if (checkedNum == 0) {
                        treeObj.siblings('.checkbox').removeClass('half_checked').removeClass('checked');
                    }
            
            if (treeObj.parents('.tree').length > 0) 
                parentStatusUpdate(treeObj.parents('.tree'));
        }
        
        function nodeStatusUpdate(nodeObj){
            if (!nodeObj.hasClass('checked')&&!nodeObj.hasClass('half_checked')) {
                if(nodeObj.siblings('ul').find('li.loseControl').length==0)
                    nodeObj.removeClass('half_checked').addClass('checked');
                else//for lose control
                    nodeObj.removeClass('checked').addClass('half_checked');
                    
                nodeObj.siblings('ul').find('li .checkbox:not(.loseControl)').addClass('checked');
            }
            else {
                nodeObj.removeClass('half_checked').removeClass('checked');
                nodeObj.siblings('ul').find('li .checkbox:not(.loseControl)').removeClass('checked');
            }
            
            if (nodeObj.parents('.tree').length > 0) 
                parentStatusUpdate(nodeObj.parents('.tree'));
        }

        //for expandable BEGIN #################
        this.collapseAll= function(dur){
          this.each(function(){
          var allChildTree = $(this).children('.tree').find('ul.tree');
          var allExpandAllow = $(this).find('div.arrow.expanded');
          doHide(allExpandAllow, allChildTree, dur);
            });
        }
        
        this.expandAll= function(dur){
          this.each(function(dur){
          var allChildTree = $(this).children('.tree').find('ul.tree');
          var allCollapseAllow = $(this).find('div.arrow.collapsed');
          doShow(allCollapseAllow, allChildTree, dur);
          });
        }
        
        function doHide(allowObj, childTree, dur){dur=dur==undefined?'fast':dur; allowObj.removeClass('expanded').addClass('collapsed'); childTree.slideUp(dur);}
        function doShow(allowObj, childTree, dur){dur=dur==undefined?'fast':dur; allowObj.removeClass('collapsed').addClass('expanded'); childTree.slideDown('fast');}
             
        function applyExpandable($container){
          var arrow = $('<div class="arrow"></div>');
          $('.checkbox', $container).before(arrow);
       
          var allTreeNode = $('ul.tree', $container);
          var collapsableNode = allTreeNode.siblings('div.arrow')
          
          allTreeNode.siblings('div.arrow').addClass('expanded');
         
         $('div.arrow.expanded, div.arrow.collapsed', $container).click(function(){
             var childTree = $(this).siblings('ul.tree');
             var allowObj = $(this);
           
             childTree.is(':visible') ? doHide(allowObj, childTree) : doShow(allowObj, childTree);
         });
        }
        //for expandable END #################
        
        function doCheckBoxTreeCompleted($container){
            //for lose control   
            if(_settings.loseControlOptions.length != 0){
                $.each(_settings.loseControlOptions, function(i, item){
                     $($container).find('[treeId=node_'+item+ ']').addClass('loseControl').parent('li').addClass('loseControl');
                });
            }

           var controlableCheckBoxs = $container.find('ul.tree .checkbox').filter(function(){
                var hasLoseControl =$(this).parents('.loseControl').length!=0
                
                if(hasLoseControl)
                $(this).addClass('loseControl');
                
                 return !hasLoseControl;
            });
            
            controlableCheckBoxs.click(function(){
                nodeStatusUpdate($(this));
            })

            if (_settings.canLabelCheck) 
                controlableCheckBoxs.siblings('label').click(function(){
                    nodeStatusUpdate($(this).siblings('.checkbox'));
                });
                
            if(_settings.isCollapsble)
            {
              applyExpandable($container);
            }
        }
        
        var _handler = function(){
           var $container = $(this);

           if (_settings.isParseTreeByJson) {
               $.ajaxSettings.async = false;
               $.getJSON(_settings.parseJson, function(data){
                   var treeMain = createNode(data);
                   $container.append(treeMain);
               });
               $.ajaxSettings.async = true;
           }
          
               doCheckBoxTreeCompleted($container);
        };
        var zyCheckTree = this.each(_handler);
        return zyCheckTree;
    };
})(jQuery);


