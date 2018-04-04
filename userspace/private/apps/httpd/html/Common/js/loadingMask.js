$.openLoadingMask = function(flag){
   
    if (window.parent.length > 0) 
        window.parent.$.openLoadingMask(flag);
};

$.closeLoadingMask = function(){
    if (window.parent.length > 0) 
        window.parent.$.closeLoadingMask();
};
