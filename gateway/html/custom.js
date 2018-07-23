var websock;
var password = false;
var maxNetworks;
var messages = [];
var webhost;

var numChanged = 0;
var numReset = 0;
var numReconnect = 0;
var numReload = 0;

var tabindex = $("#mapping > div").length * 3 + 50;
var packets;
var filters = [];

// -----------------------------------------------------------------------------
// Messages
// -----------------------------------------------------------------------------

function initMessages() {
    messages[1] = "Remote update started";
    messages[2] = "OTA update started";
    messages[3] = "Error parsing data!";
    messages[4] = "The file does not look like a valid configuration backup or is corrupted";
    messages[5] = "Changes saved. You should reboot your board now";
    messages[6] = "Home Assistant auto-discovery message sent";
    messages[7] = "Passwords do not match!";
    messages[8] = "Changes saved";
    messages[9] = "No changes detected";
    messages[10] = "Session expired, please reload page...";
}

// -----------------------------------------------------------------------------
// Utils
// -----------------------------------------------------------------------------

// http://www.the-art-of-web.com/javascript/validate-password/
function checkPassword(str) {
    // at least one number, one lowercase and one uppercase letter
    // at least eight characters that are letters, numbers or the underscore
    var re = /^(?=.*\d)(?=.*[a-z])(?=.*[A-Z])\w{8,}$/;
    return re.test(str);
}

function zeroPad(number, positions) {
    return ("0".repeat(positions) + number).slice(-positions);
}

function validateForm(form) {

    // password
    var adminPass1 = $("input[name='adminPass1']", form).val();
    if (adminPass1.length > 0 && !checkPassword(adminPass1)) {
        alert("The password you have entered is not valid, it must have at least 8 characters, 1 lower and 1 uppercase and 1 number!");
        return false;
    }

    var adminPass2 = $("input[name='adminPass2']", form).val();
    if (adminPass1 != adminPass2) {
        alert("Passwords are different!");
        return false;
    }

    return true;

}

function valueSet(data, name, value) {
    for (var i in data) {
        if (data[i]['name'] == name) {
            data[i]['value'] = value;
            return;
        }
    }
    data.push({'name': name, 'value': value});
}

function forgetCredentials() {
    $.ajax({
        'method': 'GET',
        'url': '/',
        'async': false,
        'username': "logmeout",
        'password': "123456",
        'headers': { "Authorization": "Basic xxx" }
    }).done(function(data) {
        return false;
        // If we don't get an error, we actually got an error as we expect an 401!
    }).fail(function(){
        // We expect to get an 401 Unauthorized error! In this case we are successfully
        // logged out and we redirect the user.
        return true;
    });
}

function getJson(str) {
    try {
        return JSON.parse(str);
    } catch (e) {
        return false;
    }
}

// -----------------------------------------------------------------------------
// Actions
// -----------------------------------------------------------------------------

function doReload(milliseconds) {
    milliseconds = (typeof milliseconds == 'undefined') ? 0 : parseInt(milliseconds);
    setTimeout(function() {
        window.location.reload();
    }, milliseconds);
}

function doUpdate() {

    var form = $("#formSave");

    if (validateForm(form)) {

        // Get data
        var data = form.serializeArray();

        // Post-process
        delete(data['filename']);
        $("input[type='checkbox']").each(function() {
            var name = $(this).attr("name");
            if (name) {
                valueSet(data, name, $(this).is(':checked') ? 1 : 0);
            }
        });

        websock.send(JSON.stringify({'config': data}));

        numChanged = 0;
        setTimeout(function() {
            if (numReset > 0) {
                var response = window.confirm("You have to reset the board for the changes to take effect, do you want to do it now?");
                if (response == true) doReset(false);
            } else if (numReconnect > 0) {
                var response = window.confirm("You have to reset the wifi connection for the changes to take effect, do you want to do it now?");
                if (response == true) doReconnect(false);
            } else if (numReload > 0) {
                var response = window.confirm("You have to reload the page to see the latest changes, do you want to do it now?");
                if (response == true) doReload();
            }
            numReset = numReconnect = numReload = 0;
        }, 1000);

    }

    return false;

}

function doUpgrade() {

    var contents = $("input[name='upgrade']")[0].files[0];
    if (typeof contents == 'undefined') {
        alert("First you have to select a file from your computer.");
        return false;
    }
    var filename = $("input[name='upgrade']").val().split('\\').pop();

    var data = new FormData();
    data.append('upgrade', contents, filename);

    $.ajax({

        // Your server script to process the upload
        url: webhost + 'upgrade',
        type: 'POST',

        // Form data
        data: data,

        // Tell jQuery not to process data or worry about content-type
        // You *must* include these options!
        cache: false,
        contentType: false,
        processData: false,

        success: function(data, text) {
            $("#upgrade-progress").hide();
            if (data == 'OK') {
                alert("Firmware image uploaded, board rebooting. This page will be refreshed in 5 seconds.");
                doReload(5000);
            } else {
                alert("There was an error trying to upload the new image, please try again (" + data + ").");
            }
        },

        // Custom XMLHttpRequest
        xhr: function() {
            $("#upgrade-progress").show();
            var myXhr = $.ajaxSettings.xhr();
            if (myXhr.upload) {
                // For handling the progress of the upload
                myXhr.upload.addEventListener('progress', function(e) {
                    if (e.lengthComputable) {
                        $('progress').attr({ value: e.loaded, max: e.total });
                    }
                } , false);
            }
            return myXhr;
        },

    });

    return false;

}

function doUpdatePassword() {
    var form = $("#formPassword");
    if (validateForm(form)) {
        var data = form.serializeArray();
        websock.send(JSON.stringify({'config': data}));
    }
    return false;
}

function doReset(ask) {

    ask = (typeof ask == 'undefined') ? true : ask;

    if (numChanged > 0) {
        var response = window.confirm("Some changes have not been saved yet, do you want to save them first?");
        if (response == true) return doUpdate();
    }

    if (ask) {
        var response = window.confirm("Are you sure you want to reset the device?");
        if (response == false) return false;
    }

    websock.send(JSON.stringify({'action': 'reset'}));
    doReload(5000);
    return false;

}

function doReconnect(ask) {

    ask = (typeof ask == 'undefined') ? true : ask;

    if (numChanged > 0) {
        var response = window.confirm("Some changes have not been saved yet, do you want to save them first?");
        if (response == true) return doUpdate();
    }

    if (ask) {
        var response = window.confirm("Are you sure you want to disconnect from the current WIFI network?");
        if (response == false) return false;
    }

    websock.send(JSON.stringify({'action': 'reconnect'}));
    doReload(5000);
    return false;

}

function doBackup() {
    document.getElementById('downloader').src = webhost + 'config';
    return false;
}

function onFileUpload(event) {

    var inputFiles = this.files;
    if (inputFiles == undefined || inputFiles.length == 0) return false;
    var inputFile = inputFiles[0];
    this.value = "";

    var response = window.confirm("Previous settings will be overwritten. Are you sure you want to restore this settings?");
    if (response == false) return false;

    var reader = new FileReader();
    reader.onload = function(e) {
        var data = getJson(e.target.result);
        if (data) {
            websock.send(JSON.stringify({'action': 'restore', 'data': data}));
        } else {
            alert(messages[4]);
        }
    };
    reader.readAsText(inputFile);

    return false;

}

function doRestore() {
    if (typeof window.FileReader !== 'function') {
        alert("The file API isn't supported on this browser yet.");
    } else {
        $("#uploader").click();
    }
    return false;
}


function doClearCounts() {
    websock.send(JSON.stringify({'action': 'clear-counts'}));
    return false;
}

function doFilter(e) {
    var index = packets.cell(this).index();
    if (index == 'undefined') return;
    var c = index.column;
    var column = packets.column(c);
    if (filters[c]) {
        filters[c] = false;
        column.search("");
        $(column.header()).removeClass("filtered");
    } else {
        filters[c] = true;
        var data = packets.row(this).data();
        if (e.which == 1) {
            column.search('^' + data[c] + '$', true, false );
        } else {
            column.search('^((?!(' + data[c] + ')).)*$', true, false );
        }
        $(column.header()).addClass("filtered");
    }
    column.draw();
    return false;
}

function doClearFilters() {
    for (var i = 0; i < packets.columns()[0].length; i++) {
        if (filters[i]) {
            filters[i] = false;
            var column = packets.column(i);
            column.search("");
            $(column.header()).removeClass("filtered");
            column.draw();
        }
    }
    return false;
}

function showPanel() {
    $(".panel").hide();
    $("#" + $(this).attr("data")).show();
    if ($("#layout").hasClass('active')) toggleMenu();
    $("input[type='checkbox']").iphoneStyle("calculateDimensions").iphoneStyle("refresh");
};

// -----------------------------------------------------------------------------
// Visualization
// -----------------------------------------------------------------------------

function showPanel() {
    $(".panel").hide();
    $("#" + $(this).attr("data")).show();
    if ($("#layout").hasClass('active')) toggleMenu();
    $("input[type='checkbox']").iphoneStyle("calculateDimensions").iphoneStyle("refresh");
};

function toggleMenu() {
    $("#layout").toggleClass('active');
    $("#menu").toggleClass('active');
    $("#menuLink").toggleClass('active');
}

// -----------------------------------------------------------------------------
// Templates
// -----------------------------------------------------------------------------

function addMapping() {
    var template = $("#nodeTemplate .pure-g")[0];
    var line = $(template).clone();
    $(line).find("input").each(function() {
        $(this).attr("tabindex", tabindex++);
    });
    $(line).find("button").on('click', delMapping);
    line.appendTo("#mapping");
}

function delMapping() {
    var parent = $(this).parent().parent();
    $(parent).remove();
}

// -----------------------------------------------------------------------------

function delNetwork() {
    var parent = $(this).parents(".pure-g");
    $(parent).remove();
}

function moreNetwork() {
    var parent = $(this).parents(".pure-g");
    $("div.more", parent).toggle();
}

function addNetwork() {

    var numNetworks = $("#networks > div").length;
    if (numNetworks >= maxNetworks) {
        alert("Max number of networks reached");
        return;
    }

    var tabindex = 200 + numNetworks * 10;
    var template = $("#networkTemplate").children();
    var line = $(template).clone();
    $(line).find("input").each(function() {
        $(this).attr("tabindex", tabindex++).attr("original", "");
    });
    $(line).find(".button-del-network").on('click', delNetwork);
    $(line).find(".button-more-network").on('click', moreNetwork);
    line.appendTo("#networks");

    return line;

}

// -----------------------------------------------------------------------------
// Processing
// -----------------------------------------------------------------------------

function processData(data) {

    // title
    if ("app_name" in data) {
        var title = data.app_name;
		if ("app_version" in data) {
			title = title + " " + data.app_version;
		}
        $(".pure-menu-heading").html(title);
        if ("hostname" in data) {
            title = data.hostname + " - " + title;
        }
        document.title = title;
    }

    Object.keys(data).forEach(function(key) {

        // Web Modes
        if (key == "webMode") {
            password = data.webMode == 1;
            $("#layout").toggle(data.webMode == 0);
            $("#password").toggle(data.webMode == 1);
            $("#credentials").hide();
        }

        // Actions
        if (key == "action") {

            if (data.action == "reload") {
                if (password) forgetCredentials();
                doReload(1000);
            }

            return;

        }

        // Packet
        if (key == "packet") {
            var packet = data.packet;
            var d = new Date();
            packets.row.add([
                d.toLocaleTimeString('en-US', { hour12: false }),
                packet.senderID,
                packet.packetID,
                packet.targetID,
                packet.name,
                packet.value,
                packet.rssi,
                packet.duplicates,
                packet.missing,
            ]).draw(false);
            return;
        }

        if (key == "uptime") {
            var uptime  = parseInt(data[key]);
            var seconds = uptime % 60; uptime = parseInt(uptime / 60);
            var minutes = uptime % 60; uptime = parseInt(uptime / 60);
            var hours   = uptime % 24; uptime = parseInt(uptime / 24);
            var days    = uptime;
            data[key] = days + 'd ' + zeroPad(hours, 2) + 'h ' + zeroPad(minutes, 2) + 'm ' + zeroPad(seconds, 2) + 's';
        }

        if (key == "maxNetworks") {
            maxNetworks = parseInt(data.maxNetworks);
            return;
        }

        // Wifi
        if (key == "wifi") {

            var networks = data.wifi;

            for (var i in networks) {

                // add a new row
                var line = addNetwork();

                // fill in the blanks
                var wifi = data.wifi[i];
                Object.keys(wifi).forEach(function(key) {
                    var element = $("input[name=" + key + "]", line);
                    if (element.length) element.val(wifi[key]).attr("original", wifi[key]);
                });

            }

            return;

        }

		// Topics
		if (key == "mapping") {
			for (var i in data.mapping) {

				// add a new row
				addMapping();

				// get group
				var line = $("#mapping .pure-g")[i];

				// fill in the blanks
				var mapping = data.mapping[i];
				Object.keys(mapping).forEach(function(key) {
				    var id = "input[name=" + key + "]";
				    if ($(id, line).length) $(id, line).val(mapping[key]).attr("original", mapping[key]);
				});
			}
			return;
		}

        // Messages
        if (key == "message") {
            window.alert(messages[data.message]);
            return;
        }

        // Enable options
        if (key.endsWith("Visible")) {
            var module = key.slice(0,-7);
            $(".module-" + module).show();
            return;
        }

        // Pre-process
        if (key == "network") {
            data.network = data.network.toUpperCase();
        }
        if (key == "mqttStatus") {
            data.mqttStatus = data.mqttStatus ? "CONNECTED" : "NOT CONNECTED";
        }
        if (key == "ntpStatus") {
            data.ntpStatus = data.ntpStatus ? "SYNC'D" : "NOT SYNC'D";
        }

        // Look for INPUTs
        var element = $("input[name=" + key + "]");
        if (element.length > 0) {
            if (element.attr('type') == 'checkbox') {
                element
                    .prop("checked", data[key])
                    .iphoneStyle("refresh");
            } else if (element.attr('type') == 'radio') {
                element.val([data[key]]);
            } else {
                var pre = element.attr("pre") || "";
                var post = element.attr("post") || "";
                element.val(pre + data[key] + post).attr("original", data[key]);
            }
            return;
        }

        // Look for SPANs
        var element = $("span[name=" + key + "]");
        if (element.length > 0) {
            var pre = element.attr("pre") || "";
            var post = element.attr("post") || "";
            element.html(pre + data[key] + post);
            return;
        }

        // Look for SELECTs
        var element = $("select[name=" + key + "]");
        if (element.length > 0) {
            element.val(data[key]).attr("original", data[key]);
            return;
        }

    });

}

function hasChanged() {

    var newValue, originalValue;
    if ($(this).attr('type') == 'checkbox') {
        newValue = $(this).prop("checked")
        originalValue = $(this).attr("original") == "true";
    } else {
        newValue = $(this).val();
        originalValue = $(this).attr("original");
    }
    var hasChanged = $(this).attr("hasChanged") || 0;
    var action = $(this).attr("action");

    if (typeof originalValue == 'undefined') return;
    if (action == 'none') return;

    if (newValue != originalValue) {
        if (hasChanged == 0) {
            ++numChanged;
            if (action == "reconnect") ++numReconnect;
            if (action == "reset") ++numReset;
            if (action == "reload") ++numReload;
            $(this).attr("hasChanged", 1);
        }
    } else {
        if (hasChanged == 1) {
            --numChanged;
            if (action == "reconnect") --numReconnect;
            if (action == "reset") --numReset;
            if (action == "reload") --numReload;
            $(this).attr("hasChanged", 0);
        }
    }

}

// -----------------------------------------------------------------------------
// Init & connect
// -----------------------------------------------------------------------------

function connect(host) {

    if (typeof host === 'undefined') {
        host = window.location.href.replace('#', '');
    } else {
        if (!host.startsWith("http")) {
            host = 'http://' + host + '/';
        }
    }
    webhost = host;
    wshost = host.replace('http', 'ws') + 'ws';

    if (websock) websock.close();
    websock = new WebSocket(wshost);
    websock.onopen = function(evt) {
        console.log("Connected");
    };
    websock.onclose = function(evt) {
        console.log("Disconnected");
    };
    websock.onerror = function(evt) {
        console.log("Error: ", evt);
    };
    websock.onmessage = function(evt) {
        var data = getJson(evt.data);
        if (data) processData(data);
    };
}

function init() {

    initMessages();

    $("#menuLink").on('click', toggleMenu);
    $(".pure-menu-link").on('click', showPanel);
    $('progress').attr({ value: 0, max: 100 });

    $(".button-update").on('click', doUpdate);
    $(".button-update-password").on('click', doUpdatePassword);
    $(".button-reset").on('click', doReset);
    $(".button-reconnect").on('click', doReconnect);
    $(".button-settings-backup").on('click', doBackup);
    $(".button-settings-restore").on('click', doRestore);
    $('#uploader').on('change', onFileUpload);
    $(".button-upgrade").on('click', doUpgrade);

    $(".button-upgrade-browse").on('click', function() {
        $("input[name='upgrade']")[0].click();
        return false;
    });
    $("input[name='upgrade']").change(function (){
        var fileName = $(this).val();
        $("input[name='filename']").val(fileName.replace(/^.*[\\\/]/, ''));
    });
    $(".button-add-network").on('click', function() {
        $("div.more", addNetwork()).toggle();
    });

    $(".button-add-mapping").on('click', addMapping);
    $(".button-del-mapping").on('click', delMapping);
    $(".button-clear-counts").on('click', doClearCounts);
    $(".button-clear-filters").on('click', doClearFilters);
    $('#packets tbody').on('mousedown', 'td', doFilter);

    $(document).on('change', 'input', hasChanged);
    $(document).on('change', 'select', hasChanged);

    packets = $('#packets').DataTable({
        "paging": false
    });

    for (var i = 0; i < packets.columns()[0].length; i++) {
        filters[i] = false;
    }

    $.ajax({
        'method': 'GET',
        'url': window.location.href + 'auth'
    }).done(function(data) {
        connect();
    }).fail(function(){
        $("#credentials").show();
    });

}

$(init);
