const net = require('net');
var client = new net.Socket();

const express = require('express');
const bodyParser = require('body-parser');
const router = express.Router();
const app = express();
const port = 4200;

const config = require("./config.json");
const active_days = config.days;
const days_map = ["sun", "mon", "tue", "wed", "thu", "fri", "sat"];
var bit_array = [0, 0, 0, 0, 0, 0, 0, 0];
var current_config = 0x00;

function reverse(b) {
   b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
   b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
   b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
   return b;
}

var ready = false;
function handshake(onConnect, onData) {
	console.log("Connecting...");
	client.connect(80, config.board_address, () => {
		client.on('data', (d) => {
			if(!ready) {
				ready = true;
				onConnect(d);
			}
		});	
		
		console.log("Connected to esp32!");
		client.write(Buffer.from('AA', 'hex')); //request config from esp32			
	});
}
function upload(working_byte) {
	var output_byte = 0x00;

	console.log("Reversing bit order.");
	output_byte = reverse(working_byte);

	console.log(working_byte.toString(2) + " => " + 
				 output_byte.toString(2) + " (0x" + 
				 output_byte.toString(16)+ ")");

	console.log("Sending start byte to esp32.");
	
	client.connect(80, config.board_address, () => {
		client.write(Buffer.from('80', 'hex'));
		client.write(Buffer.from(output_byte.toString(16), 'hex'));
			
		console.log("Upload Complete.");	
		console.log(output_byte.toString(16));

	});
}

function upload_array(array) {
	var working_byte = 0x00;

	console.log("Smushing Bit Array.");
	var n = array.length;
	for(var i = 0; i < n; i++) {
		var mask = 1 << i;
		if(array[i] === 1) working_byte |= mask;  
	}

	upload(working_byte);
}

router.get('/config', (req, res) => {
	var result = {
		error: false,
		data: null
	};
	
	if(!ready) {
		result.error = true;
		result["message"] = "HandshakeNotComplete";
		
		try {
			res.end(JSON.stringify(result));
		} catch (error) {
			console.log("Error sending data to device: "+error);
		}
		return;
  	}
	
	client.connect(80, config.board_address, () => {
		client.on('data', (d) => {
			var arr = Uint8Array.from(d);
			result.data = arr[0];
			try {
				res.end(JSON.stringify(result));
			} catch (error) {
				console.log("Error sending data to device: "+error);
			}
		});	
		console.log("requesting config");
		client.write(Buffer.from('AA', 'hex')); //request config from esp32			
	});


});

router.post('/reset', (req, res) => {
	client.connect(80, config.board_address, () => {
		client.write(Buffer.from('8A', 'hex'));
		res.end();
	});
});

router.post('/upload', (req, res) => {
	if(req.body && req.body.data) {
		
		
		var byte = 0x00;
		for(var i = 0; i < 8; i++) {
			var mask = 1 << i;
			if(req.body.data[i] == '1') byte |= mask;
		}
		
		upload(byte);
		res.sendStatus(200);
	} else {
		res.sendStatus(400);
	}

});

router.get('/dst', (req, res) => {
	var result = {
		error: false,
		data: null
	};
	
	if(!ready) {
		result.error = true;
		result["message"] = "HandshakeNotComplete";
		
		res.send(JSON.stringify(result));
  	} else {
		client.connect(80, config.board_address, () => {
			
		
	
			client.on('data', (d) => {
				result.data = d;
				try {
					res.end(JSON.stringify(result));
				} catch (error) {
					console.log("Error sending data to device: "+error);
				}
			});

			client.write(Buffer.from('7A', 'hex'));
		});
	}
});

app.use(express.static('web'));
app.use(bodyParser.urlencoded({extended: false}));
app.use(bodyParser.json());
app.use("/", router);
app.listen(port, () => {
  console.log(`Server listening on port ${port}`);
});

function handle_config(config) {
		console.log("Recieved config from ESP32.");
		var arr = Uint8Array.from(config);
		console.log(arr);
		console.log("Handshake Complete, configuration on device is: " + arr[0].toString(2));
		
		current_config = arr[0];
}

handshake(handle_config, (data) => {
	console.log("Recieved config from ESP32.");
	var arr = Uint8Array.from(data);
	console.log(arr);
	console.log("Configuration on device is: " + arr[0].toString(2));
	
	current_config = arr[0];
});




		
			

