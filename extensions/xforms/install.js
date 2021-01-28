// this function verifies disk space in kilobytes
function verifyDiskSpace(dirPath, spaceRequired)
{
  var spaceAvailable;

  // Get the available disk space on the given path
  spaceAvailable = fileGetDiskSpaceAvailable(dirPath);

  // Convert the available disk space into kilobytes
  spaceAvailable = parseInt(spaceAvailable / 1024);

  // do the verification
  if(spaceAvailable < spaceRequired)
  {
    logComment("Insufficient disk space: " + dirPath);
    logComment("  required : " + spaceRequired + " K");
    logComment("  available: " + spaceAvailable + " K");
    return(false);
  }

  return(true);
}

// this function deletes a file if it exists
function deleteThisFile(dirKey, file)
{
  var fFileToDelete;

  fFileToDelete = getFolder(dirKey, file);
  logComment("File to delete: " + fFileToDelete);
  if(File.isFile(fFileToDelete))
  {
    File.remove(fFileToDelete);
    return(true);
  }
  else
    return(false);
}

// this function deletes a folder if it exists
function deleteThisFolder(dirKey, folder, recursiveDelete)
{
  var fToDelete;

  if(typeof recursiveDelete == "undefined")
    recursiveDelete = true;

  fToDelete = getFolder(dirKey, folder);
  logComment("folder to delete: " + fToDelete);
  if(File.isDirectory(fToDelete))
  {
    File.dirRemove(fToDelete, recursiveDelete);
    return(true);
  }
  else
    return(false);
}

// OS type detection
// which platform?
function getPlatform()
{
  var platformStr;
  var platformNode;

  if('platform' in Install)
  {
    platformStr = new String(Install.platform);

    if (!platformStr.search(/^Macintosh/))
      platformNode = 'mac';
    else if (!platformStr.search(/^Win/))
      platformNode = 'win';
    else if (!platformStr.search(/^OS\/2/))
      platformNode = 'win';
    else
      platformNode = 'unix';
  }
  else
  {
    var fOSMac  = getFolder("Mac System");
    var fOSWin  = getFolder("Win System");

    logComment("fOSMac: "  + fOSMac);
    logComment("fOSWin: "  + fOSWin);

    if(fOSMac != null)
      platformNode = 'mac';
    else if(fOSWin != null)
      platformNode = 'win';
    else
      platformNode = 'unix';
  }

  return platformNode;
}

var err = initInstall("Mozilla XForms", "XForms", "0.6");
logComment("initInstall: " + err);

var fProgram = getFolder("Program");
var srChrome = 311;
var srComponents = 257;

if (verifyDiskSpace(fProgram, srChrome + srComponents))
{
  err = addDirectory("", "0.6", "components", fProgram, "components", true);
  logComment("addDirectory components: " + err);
  err = addDirectory("", "0.6", "chrome", fProgram, "chrome", true);
  logComment("addDirectory chrome: " + err);

  registerChrome(PACKAGE | DELAYED_CHROME, getFolder("Chrome", "xforms.jar"),
                 "content/xforms/");
  registerChrome(LOCALE | DELAYED_CHROME, getFolder("Chrome", "xforms.jar"),
                 "locale/en-US/xforms/");
  registerChrome(SKIN | DELAYED_CHROME, getFolder("Chrome", "xforms.jar"),
                 "skin/xforms/");

  if (err == SUCCESS)
  {
    err = performInstall();
    logComment("performInstall() returned: " + err);
  }
  else
  {
    cancelInstall();
    logComment("cancelInstall() due to error: "+err);
  }
}
else
  cancelInstall(INSUFFICIENT_DISK_SPACE);