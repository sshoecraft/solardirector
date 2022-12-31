// Array.forEach is available in SpiderMonkey environs ( eg. Firefox )

var arr = ["a", "b" ],
obj = {
  a: "alpha",
  b: "beta"
};

Array.forEach( arr, function( val, index, context ) { 
   console.log( val, index, context ); 
});

//a 0 ["a", "b"]
//b 1 ["a", "b"]


Array.forEach( arr, function( val, index, context ) { 
   console.log( val, index, context, this ); 
}, obj );

//a 0 ["a", "b"] Object { a="alpha", b="beta"}
//b 1 ["a", "b"] Object { a="alpha", b="beta"}


Array.forEach( arr, function( prop, index, context ) { 
   console.log( prop, this[ prop ] ); 
}, obj );

//a alpha
//b beta


//Array.forEach( Object.getOwnPropertyNames( obj ), function( prop, index, context ) { 
//   console.log( prop, this[ prop ] ); 
//}, obj );

//a alpha
//b beta


//Object.getOwnPropertyNames( obj ).forEach(function( prop, index, context ) { 
//   console.log( prop, this[ prop ] ); 
//}, obj );

//a alpha
//b beta


//var getOwn = Object.getOwnPropertyNames;

//getOwn( obj ).forEach(function( prop ) { 
//   console.log( prop, this[ prop ] ); 
//}, obj );

//a alpha
//b beta


[ "gamma", "delta" ].forEach(function( prop ) {
    obj[ prop[0] ] = prop;
});

//getOwn( obj ).forEach(function( prop ) { 
//   console.log( prop, this[ prop ] ); 
//}, obj );

//a alpha
//b beta
//g gamma
//d delta

//getOwn( obj ).filter(function( prop ) { return ~arr.indexOf( prop ); }).forEach(function( prop ) { 
//   console.log( prop, this[ prop ] ); 
//}, obj );

//a alpha
//b beta

