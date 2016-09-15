'use strict';
var placeFooterTime = function () {
    var today = new Date(),
        thisYear = today.getFullYear(),
        footer = document.getElementsByTagName("footer")[0];
    footer = footer.getElementsByTagName("p")[0];
    footer.innerHTML = footer.innerHTML[0] + " " + thisYear + footer.innerHTML.substring(1);
};

$("document").ready(function (){
    placeFooterTime();
});