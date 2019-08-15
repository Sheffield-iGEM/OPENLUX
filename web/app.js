// Wait for the page to load before requesting sensor values
document.addEventListener('DOMContentLoaded', _ => {
    updateReading();
    buildPlate(8, 12);
});

var recording = false;
var startTime = Date.now();

function recordData() {
    recording = !recording;
    if (recording) {
        startTime = Date.now()
        localStorage.clear();
        localStorage.setItem('A1', JSON.stringify([]));
    }
}

function updateReading() {
    var url = '/sensor';
    var dataDisplay = document.getElementById('data-display');
    fetch(url).then(response => {
        response.text().then(txt => {
            dataDisplay.textContent = txt;
            if (recording) {
                var data = JSON.parse(localStorage.getItem('A1'));
                console.log(data);
                data.push({
                    time: Date.now() - startTime,
                    val: Number(txt)
                });
                localStorage.setItem('A1', JSON.stringify(data));
            }
        })
    });
    window.setTimeout(updateReading, 200);
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
            var wellp = document.createElement('p');
            var text = document.createTextNode(name);
            wellp.appendChild(text);
            well.appendChild(wellp);
            well.classList.add('well');
            well.id = name;
            (function (name) {
                well.addEventListener('click', function () {
                    console.log(name);
                });
            })(name);
            plate.appendChild(well);
        }
    }
}
