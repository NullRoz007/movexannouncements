var days_input_ids = [
    ["mon-input", 1],
    ["tue-input", 2],
    ["wed-input", 3],
    ["thu-input", 4],
    ["fri-input", 5],
    ["sat-input", 6],
    ["sun-input", 0]
];

var current_config = 0;
function reverse(b) {
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
    return b;
 }

function http_get(address) {
    var xmlHttp = new XMLHttpRequest();
    xmlHttp.open( "GET", address, false );
    xmlHttp.send( null );
    return xmlHttp.responseText;
}

function http_post(address, params, done) {
    var xmlhttp = new XMLHttpRequest();

    xmlhttp.open( "POST", address, true );
    xmlhttp.setRequestHeader('Content-type', 'application/x-www-form-urlencoded');
    xmlhttp.onreadystatechange = function() {
        if(xmlhttp.readyState == 4 && xmlhttp.status == 200) {
            done();
        }
    }

    xmlhttp.send(params);
}

function get_dst() {
    var response = JSON.parse(http_get("/dst"));
    if(response.error) {
        document.getElementById("current-config").innerText += "Error." 
        return;
    }

    var dst = response.data.data[0];
    document.getElementById("dst-status").innerText = "DST Status = +" + dst + " hour(s)."
}

function get_configuration() {
    var response = JSON.parse(http_get("/config"));
    if(response.error) {
        document.getElementById("current-config").innerText += "Error." 
        return;
    }
    
    current_config = parseInt(response.data, 10);
    var text = "Current Config = " + current_config.toString(2);
    $("#current-config").text(text);
}

function set_configuration() {
    var params = "data="+$("#config-text").val();
    console.log(params);
    var ro = false;
    http_post("/upload", params, () => {
    
        if(!ro) {
            alert("Configuration Updated!");
            ro = true;
        }

        get_configuration();
    });
}

function reset_configuration() {
    var ro = false;
    http_post("/reset", "", () => {
        if(!ro) {
            alert("Configuration Reset!");
            ro = true;
        }

        get_configuration();
    });
}

function handle_input_changed() {
    var new_config = 00000000;
    for(var i = 0; i < days_input_ids.length; i++) {
        var elem_id = days_input_ids[i][0];
        var bshft_offset = days_input_ids[i][1];
        if($("#"+elem_id).is(":checked")) {
            var mask = 1 << bshft_offset;
            new_config |= mask;
        }
    }
    new_config = reverse(new_config);

    $("#config-text").val(new_config.toString(2).padStart(8, "0"));
}

$(document).ready(() => {
    get_configuration();
    setTimeout(get_dst, 1000);

    for(var i = 0; i < days_input_ids.length; i++) {
      $("#"+days_input_ids[i][0]).change(handle_input_changed);  
    }
});