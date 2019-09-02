// Wait for the page to load before requesting sensor values
var rowCount = 8;
var colCount = 12;

document.addEventListener('DOMContentLoaded', _ => {
    updateReading();
    updateTime();
    resetData();
    buildPlate(rowCount, colCount);
});

var recordQueue = -1;
var status = -1;
var activeWell = '';
var startTime = Date.now();
var syncKey = 0;

const samPer = 500;

function updateTime() {
    var time = document.getElementById('time-elapsed');
    time.textContent = Math.round((Date.now() - startTime) / 1000) + 's';
    window.setTimeout(updateTime, 200);
}

// Take a well name String at some point
function gotoWell(row, col) {
    syncKey++;
    console.log('Go! (' + row + ',' + col + ')'); 
    req = new Request('/command', {method: 'POST', body: syncKey + ';' + row + ',' + col + ';0'});
    fetch(req).then((resp) => {
        if (resp.status == 500) {
            gotoWell(row,col);
        }
    });
}

function powerLED(val) {
    syncKey++;
    console.log('Power LED: ' + val);
    var well = nameToCoords(activeWell);
    req = new Request('/command', { method: 'POST', body: syncKey + ';' + well.r + ',' + well.c + ';' + val});
    fetch(req).then((resp) => {
        if (resp.status == 500) {
            gotoWell(well.r,well.c);
        }
    });
}

function homeDevice() {
    gotoWell(0, 0);
}

function resetData() {
    startTime = Date.now();
    localStorage.clear(); // Don't clear on every refresh! Only when clear is pressed.
    var wells = document.querySelectorAll('.well');
    wells.forEach((well) => {
        well.style = null;
    });
    // changeActive('A1');
}

function resetWells() {
    var selected = document.querySelectorAll('.selected');
    selected.forEach((well) => {
        well.classList.remove('selected');
    });
}

function changeActive(well) {
    activeWell = well;
    powerLED(0);
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
    var url = '/status';
    var dataDisplay = document.getElementById('data-display');
    var statusDisplay = document.getElementById('status-display');
    fetch(url).then(response => {
        response.text().then(txt => {
            console.log('Wanted ' + syncKey + ', Got ' + Number(txt.split(';')[0]));
            if (Number(txt.split(';')[0]) == syncKey) {
                // console.log('Synckeys match! (' + syncKey + ')');
                status = Number(txt.split(';')[1]);
                // console.log('Got status: ' + status);
                var sensor = txt.split(';')[2];
                dataDisplay.textContent = sensor;
                if (recordQueue == 1) {
                    saveRecording(sensor);
                }
                if (recordQueue > 0) {
                    recordQueue--;
                }
                // console.log(recordQueue);
            }
        });
    });
    statusDisplay.textContent = translateStatus(status);
    window.setTimeout(updateReading, samPer);
}

function saveRecording(sensor) {
    var data = JSON.parse(localStorage.getItem(activeWell));
    // console.log(data);
    data.push({
        time: (Date.now() - startTime) / 1000,
        val: Number(sensor)
    });
    localStorage.setItem(activeWell, JSON.stringify(data));
}

function translateStatus(status) {
    switch(Number(status)) {
    case -1:
        return 'Waiting for status update...';
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
    // console.log('Reading wells: ');
    // console.log([...selectedWells]);
    //console.log(sortDistance([...selectedWells]));
    readWells(sortDistance([...selectedWells]));
}

function sortDistance(wells) {
    if (wells.length>1)
    {
        var sortedWells = [];
        while(wells.length > 0) {
            var currentWell = wells.shift();
            sortedWells.push(currentWell);
            wells.sort(compareDistance(currentWell));
        }
        return sortedWells;
    }
    else
    {
        return wells;
    }
    

}

function getDistance(a,b) {
    var {r:arow,c:acol} = a;
    var {r:brow,c:bcol} = b;
    var distance = Math.sqrt((brow-arow)**2+(bcol-acol)**2);
    return distance;
}

function compareDistance(r) {
    var rc = nameToCoords(r.id);
    return ((a,b) => {
        var acoor =nameToCoords(a.id);
        var bcoor =nameToCoords(b.id);

        var adist = getDistance(rc,acoor);
        var bdist = getDistance(rc,bcoor);
        var diff = adist - bdist;
        if (diff == 0) {
            return(nthWell(acoor)-nthWell(bcoor));
        }
        else
        {
            return(diff);
        }
    });
}
function nthWell(coor){
     return ((coor.r-1)*colCount+coor.c);
}

function readWells(wells) {
    if ((status == 1 && wells.length > 0) || shortCircuit) {
        status = -1;
        
        console.log('Was ready, beginning now!');
        // console.log(wells);
        var well = wells.shift();
        console.log('Active well: ' + activeWell + ' Targeted well: ' + well.id);
        if (activeWell == well.id) {
            console.log('Target well was matched');
            powerLED(1);
            recordQueue = 2; // FIXME: Better number here
        } else {
            console.log('We are not at the right well yet...');
            var {r, c} = nameToCoords(well.id);
            changeActive(well.id);
            gotoWell(r,c);
            this.shortCircuit = false;
            // console.log('nearly there');
            wells.unshift(well);
        }
    } else if (wells.length > 0 || recordQueue >= 0) {
        console.log('Status is: ' + status);
        if (recordQueue == 0) {
            powerLED(0);
            console.log("HERE");
            //this.shortCircuit = true; // I hate this...
            setActiveWellColor();
            recordQueue--;
        }
    } else {
        powerLED(0);
        return;
    }
    window.setTimeout(readWells, 1, wells);
}

function sensorToOD(val) {
    return (val-2836) / -378;
}

function ODToHue(val) {
    var norm = val / 1.75;
    var hue = 240 - norm * 240;
    return hue;
}

function sensorToHue(val) {
    var low = 1900;
    var high = 2500;
    var num = (val - low) / (high - low);
    var hue = Math.round(num * 250);
    return (Math.max(0, Math.min(250, hue)));
}

function setActiveWellColor() {
    // console.log('Setting color');
    var well = document.getElementById(activeWell);
    var wellData = JSON.parse(localStorage.getItem(activeWell));
    // console.log(wellData);
    if (wellData.length > 0) {
        // console.log("Send help");
        var rec = wellData.pop()
        // var od = sensorToOD(rec.val);
        // var odLab = document.createTextNode(od);
        //well.appendChild(odLab);
        // console.log(well);
        // console.log(od);
        well.style.backgroundColor = 'hsl(' + sensorToHue(rec.val) + ', 100%, 50%)';
    }
}
