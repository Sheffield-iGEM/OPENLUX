// Wait for the page to load before requesting sensor values
document.addEventListener('DOMContentLoaded', _ => {
    // updateReading();
    // updateStatus();
    resetData();
    buildPlate(8, 12);
});

var recording = false;
var status = 1;//0;
var activeWell = '';
var startTime = Date.now();

// Take a well name String at some point
function gotoWell(row, col) {
    console.log('Go! (' + row + ',' + col + ')'); 
    req = new Request('/goto', {method: 'POST', body: row + ',' + col});
    fetch(req).finally();
}

function toggleLED() {
    console.log('Toggle LED!');
    req = new Request('/led', { method: 'POST' });
    fetch(req).finally();
}

function resetData() {
    startTime = Date.now();
    localStorage.clear(); // Don't clear on every refresh! Only when clear is pressed.
    changeActive('A1');
}

function changeActive(well) {
    if (recording) {
        recordData();
    }
    activeWell = well;
    var wells = JSON.parse(localStorage.getItem('Wells'));
    if (wells == null) {
        wells = [];
    }
    // console.log(wells);
    // console.log(activeWell);
    if (!wells.includes(activeWell)) {
        wells.push(activeWell)
        localStorage.setItem('Wells', JSON.stringify(wells));
        localStorage.setItem(activeWell, JSON.stringify([]));
    }
}

function recordData() {
    if (recording) {
        recording = false;
        toggleLED();
    } else {
        toggleLED();
        window.setTimeout(() => recording = !recording, 1000);
    }
    console.log('Recording: ' + recording);
}

function downloadData() {
    var link = document.createElement('a');
    link.download = 'plate_readings.csv';
    var blob = new Blob([exportCSV()], {type: 'text/csv'});
    link.href = window.URL.createObjectURL(blob);
    link.click();
}

function exportCSV() {
    var wells = JSON.parse(localStorage.getItem('Wells'));
    var data = wells.map((well) => JSON.parse(localStorage.getItem(well)));
    var capRow = (row) => row.slice(-row.length + 1) + '\n';
    console.log(wells);
    var csv = wells.reduce((acc, val) => acc + ',' + 'T' + val + ',' + val, '');
    csv = capRow(csv);
    var row = null;
    do {
        rowData = data.map((arr) => arr.shift());
        console.log(rowData);
        row = rowData.reduce((acc, obj) =>
            (obj == null) ? acc + ',,' : acc + ',' + obj.time + ',' + obj.val, '');
        csv += capRow(row);
    } while (rowData.some((x) => x != null));
    return csv;
}

function updateReading() {
    var url = '/sensor';
    var dataDisplay = document.getElementById('data-display');
    fetch(url).then(response => {
        response.text().then(txt => {
            dataDisplay.textContent = txt;
            if (recording) {
                var data = JSON.parse(localStorage.getItem(activeWell));
                console.log(data);
                data.push({
                    time: (Date.now() - startTime) / 1000,
                    val: Number(txt)
                });
                localStorage.setItem(activeWell, JSON.stringify(data));
            }
        })
    });
    window.setTimeout(updateReading, 500);
}

function updateStatus() {
    var url = '/status';
    var statusDisplay = document.getElementById('status-display');
    fetch(url).then(response => {
        response.text().then(txt => {
            status = Number(txt);
            statusDisplay.textContent = translateStatus(status);
        });
    });
    window.setTimeout(updateStatus, 500);
}

function translateStatus(status) {
    switch(status) {
    case 0:
        return 'Device initialising...';
    case 1:
        return 'Ready for reading!';
    case 2:
        return 'Homing device, please wait...';
    case 3:
        return 'Moving to position...';
    case 4:
        return 'Reading absorbance...';
    case 5:
        return 'Unknown status';
    }
}

function buildPlate(rows, cols) {
    var plate = document.getElementById('wells');
    for (var r = 1; r <= rows; r++) {
        var rl = String.fromCharCode(64 + r);
        var row = document.getElementById('row-labs');
        var rlab = document.createElement('p');
        var text = document.createTextNode(rl);
        rlab.appendChild(text);
        row.appendChild(rlab);
        for (var c = 1; c <= cols; c++) {
            if (r == 1) {
                var col = document.getElementById('col-labs');
                var clab = document.createElement('p');
                var text = document.createTextNode(c);
                clab.appendChild(text);
                col.appendChild(clab);
            }
            var name = rl + c;
            var well = document.createElement('div');
            well.classList.add('well');
            well.id = name;
            wellClickListener(well, r, c, name);
            plate.appendChild(well);
        }
    }
}

function nameToCoords(name) {
    var row = name.charCodeAt(0) - 64;
    var col = Number(name.slice(1));
    return {r: row, c: col};
}

function coordToName(r,c) {
    return String.fromCharCode(64 + r) + c;
} 

function wellClickListener(well, r, c, name) {
    well.addEventListener('click', () => {
        // changeActive(name);
        // gotoWell(r,c);
        if (well.classList.contains('selected')) {
            well.classList.remove('selected');
        } else {
            well.classList.add('selected');
        }
    });
}

function readSelected() {
    var selectedWells = document.querySelectorAll('.well.selected');
    console.log('Reading wells: ');
    console.log([...selectedWells]);
    readWells([...selectedWells]);
}

function readWells(wells) {
    if (status == 1 && wells.length > 0) {
        console.log('Here');
        console.log(wells);
        var well = wells.shift();
        console.log('Active well: ' + activeWell);
        if (activeWell == well.id) {
            console.log('activewell');
            recordData();
            window.setTimeout(recordData, 3000);
            window.setTimeout(setActiveWellColor, 3100);
            window.setTimeout(readWells, 3100, wells);
        } else {
            console.log('else');
            var {r, c} = nameToCoords(well.id);
            changeActive(well.id);
            gotoWell(r,c);
            console.log('nearly there');
            wells.unshift(well)
            window.setTimeout(readWells, 1000, wells);
        }
    }
}

function sensorToOD(val) {
    return (val-2836) / -378;
}

function ODToHue(val) {
    var norm = val / 2.0;
    var hue = norm * 240;
    return hue;
}
    
function setActiveWellColor() {
    console.log('Setting color');
    var well = document.getElementById(activeWell);
    var wellData = JSON.parse(localStorage.getItem(activeWell));
    if (wellData.length > 0) {
        var rec = wellData.pop()
        well.style.backgroundColor = 'hsl(' + ODToHue(rec.val) +', 100, 50)';
    }
    well.style.backgroundColor = 'black';
}
