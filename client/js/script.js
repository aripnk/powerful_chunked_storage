(function() {
var f = document.getElementById('f');

var server = 'http://localhost:7000'
var ret = false;
var retdata;

if (f.files.length)
  processFile();

f.addEventListener('change', processFile, false);

function processFile(e) {
  var file = f.files[0];
  var size = file.size;
  var sliceSize = 1024000;
  var nchunks = Math.ceil(size / sliceSize);
  var filename = file.name.replace(/[^A-Z0-9]/ig,"_");
  var type = file.type;
  var start = 0;
  var iter = 0;
  var mkey;
  var key = 0;
  var lock = false;
  ret = false;
  retdata = '';


  setTimeout(loop, 1);

  function loop() {
    var end = start + sliceSize;

    if (size - end < 0) {
      end = size;
    }

    var s = slice(file, start, end);

    if (start == 0){
      if (lock == false){
        send_first_chunk(s, key, filename, size, type, nchunks);
        lock = true;
      }else{
        if (ret){
          var data = JSON.parse(retdata);
          if (data.status == 0 && end < size){
            start += sliceSize;
            if (iter == 0) mkey = data.key;
            ++iter;
            key = mkey+'.'+iter;
            lock = false;
          }
        }
      }
      if (end < size) setTimeout(loop, 5);
    }else{
      if (lock == false){
        send_chunk(s, key);
        lock = true;
      }else{
        if (ret){
          var data = JSON.parse(retdata);
          if (data.status == 0 && end < size){
            start += sliceSize;
            ++iter;
            key = mkey+'.'+iter;
            lock = false;
          }
        }
      }
      if (end < size) setTimeout(loop, 5);
      else upload_success(filename, mkey);
    }
  }
}

function send_chunk(piece, key) {
  ret = false;
  var xhr = new XMLHttpRequest();

  var query = server + '/upload?key=' + key;

  xhr.open('POST', query, true);

  xhr.setRequestHeader("Content-type", "application/x-www-form-urlencoded");

  xhr.onreadystatechange = function() {
    if (xhr.readyState == XMLHttpRequest.DONE) {
      retdata = xhr.responseText;
      ret = true;
    }
  }

  xhr.send(piece);
}

function send_first_chunk(piece, key, filename, size, type, nchunks) {
  ret = false;
  var xhr = new XMLHttpRequest();

  var query = server + '/upload?key='+key
              +'&filename='+filename
              +'&size='+size
              +'&type='+type
              +'&nchunks='+nchunks;

  xhr.open('POST', query, true);

  xhr.setRequestHeader("Content-type", "application/x-www-form-urlencoded");

  xhr.onreadystatechange = function() {
    if (xhr.readyState == XMLHttpRequest.DONE) {
      retdata = xhr.responseText;
      ret = true;
      var data = JSON.parse(retdata);
      if (nchunks == 1 && data.status == 0){
        upload_success(filename, data.key);
      }
    }
  }

  xhr.send(piece);
}

function upload_success(filename, link){
  document.getElementById("upload-form").reset();
  document.getElementById("uploaded").innerHTML += "<h3><a href=\""+server+"/download?"+link+"\" target=\"_blank\" style=\"color:white;\">"+filename+"</a></h3>";
}

function slice(file, start, end) {
  var slice = file.mozSlice ? file.mozSlice :
              file.webkitSlice ? file.webkitSlice :
              file.slice ? file.slice : noop;

  return slice.bind(file)(start, end);
}

function noop() {

}

})();
