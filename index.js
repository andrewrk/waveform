var execFile, waveform_bin, path;

path = require('path');

execFile = require('child_process').execFile;
waveform_bin = path.resolve(__dirname, "bin", "waveform");

module.exports = function(audiofile, pngfile, options, callback) {
  var cmdline, value, opt;

  cmdline = [audiofile, pngfile];
  for (opt in options) {
    if (options.hasOwnProperty(opt)) {
      value = options[opt];
      cmdline.push('--' + opt);
      cmdline.push(value);
    }
  }
  
  execFile(waveform_bin, cmdline, function(err, stdout, stderr) {
    var myErr;
    if (err) {
      myErr = new Error("waveform binary returned error"); 
      myErr.stdout = stdout;
      myErr.stderr = stderr;
      myErr.internal = err;
      callback(myErr);
    } else {
      callback();
    }
  });
};
