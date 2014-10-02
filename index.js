var path = require('path');
var execFile = require('child_process').execFile;
var waveformBin = path.resolve(__dirname, "bin", "waveform");

var argNames = [ 'width', 'height', 'color-bg', 'color-center', 'color-outer' ];
var flagNames = [ 'scan' ];

module.exports = function(audiofile, pngfile, options, callback) {
  var cmdline = [audiofile, pngfile];
  argNames.forEach(function(argName) {
      if (options.hasOwnProperty(argName)) {
          var value = options[argName];
          cmdline.push('--' + argName);
          cmdline.push(value);
      }
  });
  flagNames.forEach(function(flagName) {
      if (options[flagName]) {
          cmdline.push('--' + flagName);
      }
  });

  execFile(waveformBin, cmdline, function(err, stdout, stderr) {
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
