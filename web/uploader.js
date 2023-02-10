const http = require('http');
const net = require('net');
var client = new net.Socket();

const config = require("./config.json");
const active_days = config.days;
const days_map = ["sun", "mon", "tue", "wed", "thu", "fri", "sat"];
var bit_array = [0, 0, 0, 0, 1, 1, 1, 1];
var current_config = 0x00;

function reverse(b) {
   b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
   b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
   b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
   return b;
}

console.log("Reading Active Days.");
var output_byte = 0x00;
var working_byte = 0x00;
var indicies = [];
for(var i = 0; i < active_days.length; i++) {
    var j = days_map.indexOf(active_days[i]);
    bit_array[j] = 1;
    indicies.push(j);
}



console.log("Smushing Bit Array.");
var n = bit_array.length;
for(var i = 0; i < n; i++) {
    var mask = 1 << i;
    if(bit_array[i] === 1) working_byte |= mask;  
}

console.log("Reversing bit order.");
output_byte = 0b11001111;

console.log(working_byte.toString(2) + " => " + 
                output_byte.toString(2) + " (0x" + 
                output_byte.toString(16)+ ")");

console.log("Sending byte to esp32.");

client.connect(80, config.board_address, () => {
    client.write(Buffer.from(output_byte.toString(16), 'hex'));
    console.log("Upload Complete.");	
});	






