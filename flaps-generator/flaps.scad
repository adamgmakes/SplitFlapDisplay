// Flap Generator for splitflap design based on: https://github.com/adamgmakes/SplitFlapDisplay
//
// Flap generator created by Richard Garsthagen (the.anykey@gmail.com)
// License under creative commons: https://creativecommons.org/licenses/by-nc-sa/4.0/

$fn=180; // Quality of render

layers = 3;
layerheight = 0.16;
fontsize = 28;
blackmargin = 3;

// USE [F6] to render the flaps

// Make all flaps for an individual color layer
//MakeFlaps(0);

// Make a single flap for an individual color layer
//MakeSingleFlap(12,0); // Flap Number, Color Layer

// Show Preview of all the flaps - NOT FOR PRINTING
PreviewFlaps();



// Fonts to use
fonts = ["Consolas:style=bold", "Arial:style=Narrow Bold"];
charFont = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];

// 64 Characters you want to use
chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789?!@#$&[]-+=:%'\u20AC\"\u2191\u2193\u20BF\u00b0\u263A.♥    ";

// Flap Color layer, to generate as individual colors for each flap background
flapColor = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4,3,2,1,0];

// Color layer, to generate as individual colors
charColorLayer = [1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,2,2,4,1,4,4,4,1,1,1,1,1,1,1,1,3,2,4,1,4,1,2,1,1,1,1,1];

// Per Character Font Size overwrite
charSizeOffset = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,-4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5,5,0,0,0,0,0,0,0,0,0,0,0];

// Per Character Y Position overwrite -> default is centered
charYposOffset = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,-3.5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,9,0,9,0,0,0,9,1.5,-12,0,0,0,0,0,0];

colors = ["black", "white", "red", "green", "yellow"];

module PreviewFlaps(){
    for ( y = [0 : 7] ){
        for ( x = [0 : 7] ){
            char = (y*8)+x;
            translate([34+(x*34),22+(y*43),0])
            flapPreview(char); 
        }
    }
    
}

module MakeSingleFlap(char, part){
    
            if (char<64) {
                if (char==0){
                 translate([17+(x*17),22+(y*43),0])
                 flap(63, char, char+1, part); }
                else if (char==63) {
                 translate([17+(x*17),22+(y*43),0])
                 flap(char-1, char, 0, part); }
                else {
                 translate([17+(x*17),22+(y*43),0])
                 flap(char-1, char, char+1, part);} 
            }
            


}

module MakeFlaps(part){
    for ( y = [0 : 5] ){
        for ( x = [0 : 2 : 12] ){
            char = (y*14)+x;
            if (char<64) {
                if (char==0){
                 translate([17+(x*17),22+(y*43),0])
                 flap(63, char, char+1, part); }
                else if (char==63) {
                 translate([17+(x*17),22+(y*43),0])
                 flap(char-1, char, 0, part); }
                else {
                 translate([17+(x*17),22+(y*43),0])
                 flap(char-1, char, char+1, part);} 
            }
            
        }
    }
}

module flapPreview(c1){
     difference(){ 
         union(){
         color(colors[flapColor[c1]])
         linear_extrude(h=(layers*layerheight))
         import("flap.dxf");
         
         color(colors[flapColor[c1]])
             difference(){
             linear_extrude(h=(layers*layerheight))
             rotate([0,0,180])
             import("flap.dxf");

             translate([0,-21.16+(blackmargin/2),(layerheight*layers)/2])            
             cube([34,blackmargin,layerheight*layers], center=true);
         }

         // black margin
             color(colors[0])
             translate([0,0,layerheight*2])
             difference(){
                linear_extrude(h=(layerheight))
                 rotate([0,0,180])
                 import("flap.dxf");
                 translate([-17,-21.16+blackmargin,0])            
                 cube([34,21.16-blackmargin,layerheight]);
         }
         
         }
         charPreview(c1);
     }
    charPreview(c1); 
}

module flap(c1,c2,c3, part){
    //print flaps with character cutout
    
     difference(){ 
     union(){
     if (flapColor[c3]==part) {
         
         //color(colors[flapColor[c3]])
         //linear_extrude(h=(layerheight))
         //import("flap.dxf");
         
         if (flapColor[c3] != 0) {
         color(colors[flapColor[c3]])
         translate([0,0,0])
             difference(){
                 linear_extrude(h=(layerheight))
                 import("flap.dxf");
                 translate([0,21.16-(blackmargin/2),layerheight/2])            
                 cube([34,blackmargin,layerheight], center=true);
                 
             }
         }
         else {
             color(colors[0])
             translate([0,0,0])
             linear_extrude(h=(layerheight))
             import("flap.dxf");
         } 
         
         
     }
     
     if (part==0){  // Always generate middle layer black
      color(colors[0])
      translate([0,0,layerheight])
      linear_extrude(h=(layerheight))
      import("flap.dxf");
     }
     
     if (flapColor[c2]==part) {
         color(colors[flapColor[c2]])
         translate([0,0,layerheight*2])
         linear_extrude(h=(layerheight))
         import("flap.dxf");
     }
     
     if (flapColor[c1]==part) {
         color(colors[flapColor[c1]])
         linear_extrude(h=(layerheight))
         rotate([0,0,180])
         import("flap.dxf");
     }
         
     if (part==0){
      color(colors[0])
      translate([0,0,layerheight])
      linear_extrude(h=(layerheight))
      rotate([0,0,180])
      import("flap.dxf");
      
      
      //top layer bottom margin
      color(colors[0])
             translate([0,0,layerheight*2])
             difference(){
                linear_extrude(h=(layerheight))
                 rotate([0,0,180])
                 import("flap.dxf");
                 translate([-17,-21.16+blackmargin,0])            
                 cube([34,21.16-blackmargin,layerheight]);
         }
      
      //bottom layer bottom margin
      color(colors[0])
             translate([0,0,0])
             difference(){
                linear_extrude(h=(layerheight))
                 import("flap.dxf");
                translate([-17,-21.16+blackmargin,0])            
                 cube([34,21.16-blackmargin,layerheight]);
         }
         
         
      
     }
     
     if (flapColor[c2]==part) {
         if (flapColor[c2] != 0) {
         color(colors[flapColor[c2]])
         translate([0,0,layerheight*2])
             difference(){
                 linear_extrude(h=(layerheight))
                 rotate([0,0,180])
                 import("flap.dxf");
                 translate([0,-21.16+(blackmargin/2),layerheight/2])            
                 cube([34,blackmargin,layerheight], center=true);
                 
             }
         }
         else {
             color(colors[0])
             translate([0,0,layerheight*2])
             linear_extrude(h=(layerheight))
             rotate([0,0,180])
             import("flap.dxf");
         } 
     
         
     }
     }
     char1(c1);
     char2(c2);
     char3(c3);
     }
    

    //print just the characters
    if (charColorLayer[c1] == part) { char1(c1); }
    if (charColorLayer[c2] == part) { char2(c2); }
    if (charColorLayer[c3] == part) { char3(c3); }

}

module charPreview(c){
difference(){
     color(colors[charColorLayer[c]])
     translate([0,charYposOffset[c],layerheight*(layers-1)])
     linear_extrude(h=layerheight)
     text(chars[c], size=fontsize+charSizeOffset[c], font=fonts[charFont[c]], halign="center", valign="center");
     
     translate([-20,-0.25,layerheight*(layers-1)])
     cube([50,0.5,layerheight]);
}
}

module char1(c){
 difference(){
     color(colors[charColorLayer[c]])
     translate([0,-charYposOffset[c],0])
     linear_extrude(h=layerheight)
     rotate([180,0,0])
     text(chars[c], size=fontsize+charSizeOffset[c], font=fonts[charFont[c]], halign="center", valign="center");
     
     translate([-20,-0.25,0])
     cube([50,20,layerheight]);
 }
}

module char2(c){
difference(){
     color(colors[charColorLayer[c]])
     translate([0,charYposOffset[c],layerheight*(layers-1)])
     linear_extrude(h=layerheight)
     text(chars[c], size=fontsize+charSizeOffset[c], font=fonts[charFont[c]], halign="center", valign="center");
     
     translate([-20,-0.25,layerheight*(layers-1)])
     cube([50,0.5,layerheight]);
     
}
}

module char3(c){
 difference(){
     color(colors[charColorLayer[c]])
     translate([0,-charYposOffset[c],0])
     linear_extrude(h=layerheight)
     rotate([180,0,0])
     text(chars[c], size=fontsize+charSizeOffset[c], font=fonts[charFont[c]], halign="center", valign="center");
     
     translate([-20,-20+0.25,0])
     cube([50,20,layerheight]);
 }
}




