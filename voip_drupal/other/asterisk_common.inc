<?php
/* $id$ */

/**
 * @file
 *   Provides functions for other files.
 *
 *   Just the asterisk_cron.php cron script uses it now, but in the future the
 *   asterisk.module and php AGI scripts will use it as well.
 */

/*************************************************/
/****** Error handling functionality *************/
/*************************************************/
static $ast_is_error = FALSE;
static $ast_error_message = "";

/*
 * Handles asterisk error messages.
 *
 * @param
 *   string error message
 * @return
 *   void
 */
function asterisk_error($message = NULL) {

  global $ast_is_error;
  global $ast_error_message;

  if ($message) {
    $ast_is_error = TRUE;
    $ast_error_message = $message;
  }
  return;
}

/*
 * Resets asterisk error handler
 *
 * @return
 *   void
 */
function asterisk_error_reset() {
  global $ast_is_error;

  $ast_is_error = FALSE;
}

/*
 * Returns the last asterisk error message.
 *
 * @return
 *   string asterisk error message or "" in case of no error
 */
function asterisk_error_msg() {
  global $ast_is_error;
  global $ast_error_message;

  $rc = ($ast_is_error) ? $ast_error_msg : "";
  return $rc;
}

/**
 * Get all the calls from a server and dispatch them
 */
function asterisk_call_queued($server) {
  $calls = xmlrpc($server['url'], 'asterisk.queue.getNextCalls', $server['name'], $server['pass']);
  if (is_array($calls)) {
    foreach ($calls as $call) {
      if ($call['caller_number'] == 'system' && $call['callee_number'] == 'system') {
        asterisk_system_call($server, $call);
      }
      else {
        asterisk_create_call_file($server, $call);
      }
    }
    return count($calls);
  }
  else {
    //print xmlrpc_error_msg();
    return 0;
  }
}

/**
 * Formats a number so that our dialplan and asterisk can understand it.
 * The '@' character is removed to disallow numbers such as name@voipjet/011...
 * @param $number
 *   Number to format
 * @return
 *   The formatted number
 */
function format_number($number) {
  if (strpos($number, '://') !== FALSE) {
    $badchars = array(' ', '(', ')', '+', ':', '/', '@');
  }
  else {
    $badchars = array(' ', '(', ')', '+', '-', ':', '/', '@');
  }
  $number =  str_replace($badchars, '', $number);
  if (is_numeric($number)) {
    if (strlen($number) == 10) {
      // this looks like an NANPA number, lets prefix it with a '1'
      $number = '1'.$number;
   }
    elseif (strlen($number) == 12) {
      // this does not look like an NANPA number, lets prefix it with '011'
      $number = '011'.$number;
    }
  }
  return $number;
}

/**
 * Executes an asterisk system call
 * @param $server
 *   Information pertaining to the server from which this call originated
 * @param $call
 *   The call array which will be used to generate the call file
 */
function asterisk_system_call($server, $call) {
  global $asterisk, $confdir;
  $vars = $call['vars'];
  if (!is_object($vars['user'])) {
    $vars['user'] = array2object($vars['user']);
  }
  if ($vars['op']) {
    switch ($vars['op']) {

      // iax.conf, sip.conf, voicemail.conf stuff
      case 'update-user':
      case 'create-user':
        asterisk_update_asterisk_user($server, $vars['user'], 'iax');
        asterisk_update_asterisk_user($server, $vars['user'], 'sip');
        asterisk_update_asterisk_user($server, $vars['user'], 'vm');
        break;
      case 'delete-user':
        asterisk_delete_asterisk_user($server, $vars['user'], 'iax');
        asterisk_delete_asterisk_user($server, $vars['user'], 'sip');
        asterisk_delete_asterisk_user($server, $vars['user'], 'vm');
        break;
    }
    // recreate the include file helpers
    asterisk_recreate_files($confdir);

    // reload the asterisk server
    system("$asterisk -rx reload");
  }
}

/**
 * For old asterisk installations that do not support #include configuration globbing
 * We use is for new asterisk installations too! (works there as well)
 */
function asterisk_recreate_files($confdir) {
  $dh = opendir($confdir);
  $iax_conf_include = '';
  $sip_conf_include = '';
  $vm_conf_include  = '[drupal]'.chr(10); 
  while (FALSE !== ($file = readdir($dh))) {
    if ($file != '.' && $file != '..') {
      if (is_file($confdir.DIRECTORY_SEPARATOR.$file.DIRECTORY_SEPARATOR.'iax.conf')) {
        $iax_conf_include .= "#include drupal/$file/iax.conf\n";
      }
      if (is_file($confdir.DIRECTORY_SEPARATOR.$file.DIRECTORY_SEPARATOR.'sip.conf')) {
        $sip_conf_include .= "#include drupal/$file/sip.conf\n";
      }
      if (is_file($confdir.DIRECTORY_SEPARATOR.$file.DIRECTORY_SEPARATOR.'vm.conf')) {
        $vm_conf_include .= file_get_contents($confdir.DIRECTORY_SEPARATOR.$file.DIRECTORY_SEPARATOR.'vm.conf');
      }
    }
  }
  asterisk_write_file($iax_conf_include, $confdir.DIRECTORY_SEPARATOR.'iax.conf');
  asterisk_write_file($sip_conf_include, $confdir.DIRECTORY_SEPARATOR.'sip.conf');
  asterisk_write_file($vm_conf_include, $confdir.DIRECTORY_SEPARATOR.'voicemail.conf');
}

/**
 * Updates an asterisk iax or sip user
 */
function asterisk_update_asterisk_user($server, $user, $type = 'iax') {

  global $confdir;

  if (!$user->uid) {
    return;
  }

  $uri = parse_url($server['url']);
  $host = $uri['host'];

  // create the configuration directory
  $user_confdir = $confdir.DIRECTORY_SEPARATOR.$user->uid.'-'.$host;
  if (!is_dir($user_confdir)) {
    mkdir($user_confdir);
  }
  
  // create the configuration file
  switch ($type) {
    case 'iax':
      $conf = asterisk_create_iax_conf($server, $user);
      break;
    case 'sip':
      $conf = asterisk_create_sip_conf($server, $user);
      break;
    case 'vm':
      $conf = asterisk_create_voicemail_conf($server, $user);
      break;
    default:
      // lets not do anything if we don't know what's going on
      return;
  }
  // write the configuration file
  asterisk_write_file($conf, $user_confdir.DIRECTORY_SEPARATOR.$type.'.conf');
}

/**
 * Deletes the iax configuration for a single user
 */
function asterisk_delete_asterisk_user($server, $user, $type) {
  global $confdir;

  // sanity check
  if (!$user->uid) {
    return;
  }

  $uri = parse_url($server['url']);
  $host = $uri['host'];

  // delete the configuration files
  $user_confdir = $confdir.DIRECTORY_SEPARATOR.$user->uid.$host;

  unlink ($user_confdir.DIRECTORY_SEPARATOR.$type.'.conf');
}

/**
 * Creates the asterisk call file
 * @param $server
 *   Information pertaining to the server from which this call originated
 * @param $call
 *   The call array which will be used to generate the call file
 */
function asterisk_create_call_file($server, $call) {
  global $tempdir, $callspooldir, $def_channel, $debug, $recorddir;
  
  // Format the numbers
  $call['caller_number'] = format_number($call['caller_number']);
  $call['callee_number'] = format_number($call['callee_number']);

  // Make a uniquish string for later use
  $account = make_account($server, $call['uid']);

  $callfile  = '';

  // Use a local Asterisk context/extension to call out with
  $callfile .= 'Channel: Local/'.$call['caller_number'].'@'.$call['context'].chr(10);

  $callfile .= 'Callerid: '  .$call['caller_id']     .chr(10);
  $callfile .= 'MaxRetries: '.$call['max_retries']   .chr(10);
  $callfile .= 'RetryTime: ' .$call['retry_time']    .chr(10);
  $callfile .= 'WaitTime: '  .$call['wait_time']     .chr(10);
  //$callfile .= 'Account: '   .$account               .chr(10);
  $callfile .= 'Context: '   .$call['context']       .chr(10);
  $callfile .= 'Context: '   .'drupal'               .chr(10);
  $callfile .= 'Extension: ' .$call['callee_number'] .chr(10);
  $callfile .= 'Priority: '  .$call['priority']      .chr(10);

  // Add all the variables to the call file
  if (isset($call['vars']) && is_array($call['vars'])) {
    if (array_key_exists('fid', $call['vars'])) {
      switch ($call['callee_number']) {
        case 'playback':
          $call['vars'] = array_merge($call['vars'], fetch_file($server, $call));
          break;
        case 'record':
          $call['vars']['filename'] = $recorddir.DIRECTORY_SEPARATOR.
                                      make_account($server, $call['vars']['fid']).
                                      '_'.$call['vars']['nid'].'_'.basename($call['vars']['filename']);
          $call['vars'] = array_merge($call['vars'], array('format' => 'wav'));
      }
    }
    foreach ($call['vars'] as $key => $value) {
      $callfile .= 'SetVar: '.$key.'='.$value.chr(10);
    }
  }

  if ($debug) {
    print chr(10).'-------------'.chr(10).$callfile.chr(10);
  }

  $prefix = $account.'-';

  // write the information to a temporary file`
  $tmpfname = tempnam('tmp', $prefix);

  if (asterisk_write_file($callfile, $tmpfname)) {
    // move (NOT copy or create in) to outgoing spool
    if (rename($tmpfname, $callspooldir.DIRECTORY_SEPARATOR.basename($tmpfname))) {
      return TRUE;
    }
  }
  return FALSE;
}

/**
 * Get the audio files that have been recorded for a drupal site
 */
function get_recorded_messages($server) {
  global $recorddir;
  $filenames = array();
  $dh = opendir($recorddir.DIRECTORY_SEPARATOR);
  while (($filename = readdir($dh)) !== FALSE) {
    if (strpos($filename, make_account($server)) === 0) {
      $filenames[] = $filename;
    }
  }
  return $filenames;
}

/**
 * Uploads the recorded files for a given drupal site
 */
function upload_recorded_messages($server, $filenames) {
  global $recorddir;
  foreach ($filenames as $filename) {
    $file = array();
    $file['bits'] = '';

    $filepath = $recorddir.DIRECTORY_SEPARATOR.$filename;

    $name = basename($filename);

    $file = array_merge($file, parse_recorded_filename($server, $filename));
    $file['filesize'] = filesize($filepath);

    $fh = fopen($filepath, 'r');
    while (!feof($fh)) {
      $file['bits'] .= fread($fh, 8192);
    }
    fclose($fh);

    $file['bits'] = xmlrpc_base64($file['bits']);
    $file['filemime'] = 'audio/x-wav';

    print $server['url'].chr(10);

    xmlrpc($server['url'], 
           'asterisk.uploadFile',
           $server['name'],
           $server['pass'],
           $file
          );
    unlink($recorddir.DIRECTORY_SEPARATOR.$filename);
  }
}

/**
 * Parses a recorded audio filename for metadata
 */
function parse_recorded_filename($server, $filename) {
  $file = array();
  $account = make_account($server);
  $name = substr($filename, strlen($account));
  $bits = explode('_', $name);
  $file['fid'] = $bits[0] ? $bits[0] : 0;
  $file['nid'] = $bits[1] ? $bits[1] : 0;
  $file['filename'] = substr($name, strlen($file['nid'].$file['fid']) + 2);
  return $file; 
}

/**
 * Fetches a file from the server originating the call
 * @param $server
 *   Information pertaining to the server from which this call originated
 * @param $call
 *   The call array which will be used to generate the call file
 * @return
 *   An array with the file information. This corresponds to the schema of the {files} table
 */
function fetch_file($server, $call) {
  global $server, $tempdir;
  $fid = $call['vars']['fid'];
  //get the file information
  $file = xmlrpc($server['url'], 'asterisk.getFileInfo', $server['name'], $server['pass'], $fid);
 
  $account = make_account($server, $call['uid']);
  $filename = $tempdir.DIRECTORY_SEPARATOR.$account.'-'.$fid.'-'.$file['filesize'].'-'.$file['filename'];

  // Please do not continue if this is not a audio file
  if (strpos($file['filemime'], 'audio') === FALSE) {
    return FALSE;
  }

  // check if we have downloaded the file already
  if (is_file($filename.'.wav') && is_readable($filename.'.wav')) {
    // file is already downloaded. We don't need to do anything else

  } else {
    $temp_filename = tempnam($tempdir, $account);
    $temp_file   = fopen($temp_filename, 'w');

    // try to work around filenames with odd characters
    $filename_pos = strrpos($file['url'], '/') + 1;
    $remote_filename = rawurlencode(substr($file['url'], $filename_pos));
    $url = substr($file['url'], 0, $filename_pos).$remote_filename;
    print "$url\n";

    $remote_file = fopen($url, 'r');

    if ($remote_file && $temp_file) {

      // save the remote file to a temporary file
      while (!feof($remote_file)) {
        $buffer = fgets($remote_file, 4096);
        fwrite($temp_file, $buffer);
      }
      fclose($remote_file);
      fclose($temp_file);

      // Asterisk uses 8kHz mono wav
      // Asterisk will .wav to the filename when searching
      convert_sound_file($temp_filename, $file['filemime'], $filename.'.wav');
      unlink($temp_filename);
    }
  }

  // We use the FILENAME variable in our dialplan
  $file['FILENAME'] = $filename;

  return $file;
}

/**
 * Converts an mp3 or regular wav file to an Asterisk compatible wav file
 * @param $orig_file
 *   Fullpath to the original file
 * @param $orig_filemime
 *   The mimetype of the original file
 * @param $converted_file
 *   (Desired) Fullpath to the converted file
 * @return
 *   The return value of the conversion program (mpg123 or sox)
 */
function convert_sound_file($orig_file, $orig_filemime, $converted_file) {
//echo ("convert_sound_file($orig_file, $orig_filemime, $converted_file)");
  global $tempdir, $sox, $mpg123;

  // Try to detect an mp3 file. If it is an mp3 file, then convert it to a wav
  // We don't make the wav directly in case we are actually using mpg321
  // (which doesn't have all these options)
  //system("$mpg123 -q --mono -r 8000 -f 4096 -w $temp_file $orig_file", $ret);
  if (strpos($orig_filemime, 'mp') !== FALSE) {
    $temp_file = tempnam($tempdir, 'drupal_mp3_convert');
    system("$mpg123 -q -m -w '$temp_file' '$orig_file'", $ret);
    $orig_file = $temp_file;
  }
  // We have a wav file, convert it to an Asterisk compatible wav file
  //exec("$sox $orig_file -r 8000 -c 1 -w $converted_file resample -ql", $output);
//echo("$sox '$orig_file' -r 8000 -c 1 -w '$converted_file'");
  system(escapeshellcmd("$sox '$orig_file' -r 8000 -c 1 -w '$converted_file'"), $ret);

  // If there is a temp file, delete it
  if (isset($temp_file)) {
    unlink($temp_file);
  }
//echo("return: $ret");
  return $ret;
}

/**
 * Makes a uniquish string that can be used to identify files relating to a call
 * @param $server
 *   Information pertaining to the server from which this call originated
 * @param $call
 *   The call array which will be used to generate the call file
 * @return
 *   A uniquish string related to the server and call details
 */
function make_account($server, $id = '') {
  $uri     = parse_url($server['url']);
  $account = $uri['host'].'_'.'drupal'.'-'.$id;
  return $account;
}

 /**
 * Create an sip.conf include for one user
 * @param $user
 *   object The user object to create the sip configuration for
 * @return
 *   string A string containing the iax.conf configuration include
 */
function asterisk_create_sip_conf($server, $user) {

  $uri = parse_url($server['url']);
  $host = $uri['host'];

  $conf  = ";;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;\n";
  $conf .= ";{$user->name} (uid:{$user->uid}, host:$host)\n";
  $conf .= ";;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;\n";
  $conf .= ";Created by Drupal\n";
  $conf .= "[{$user->name}-{$host}]\n";
  $conf .= "type=friend\n";
  $conf .= "secret={$user->pin}\n";
  $conf .= "host=dynamic\n";
  $conf .= "callerid=\"{$user->name}\" <{$user->uid}-{$host}>\n";
  $conf .= "disallow=all\n";
  $conf .= "allow=gsm\n";
  $conf .= "allow=ulaw\n";
  $conf .= "context=drupal\n";
  $conf .= "transfer=yes\n";
  $conf .= "canreinvite=yes\n";
  $conf .= "dtmfmode=rfc2833\n\n";
  //$sipconf .= "qualify=yes\n";
  //$sipconf .= "nat=no\n\n";

  return $conf;
}

/**
 * Create an iax.conf include for one user
 * @param $user
 *   object The user object to create the iax configuration for
 * @return
 *   string A string containing the iax.conf configuration include
 */
function asterisk_create_iax_conf($server, $user) {

  $uri = parse_url($server['url']);
  $host = $uri['host'];

  $conf  = ";;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;\n";
  $conf .= ";{$user->name} (uid:{$user->uid}, host:$host)\n";
  $conf .= ";;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;\n";
  $conf .= ";Created by Drupal\n";
  $conf .= "[{$user->name}-{$host}]\n";
  $conf .= "type=friend\n";
  $conf .= "host=dynamic\n";
  $conf .= "callerid=\"{$user->name}\" <{$user->uid}-{$host}>\n";
  $conf .= "context=drupal\n";
  $conf .= "secret={$user->pin}\n";
  $conf .= "\n";

  return $conf;
}

/**
 * Create an voicemail.conf include for one user
 * @param $user
 *   object The user object to create the iax configuration for
 * @return
 *   string A string containing the voicemail.conf configuration include
 */
function asterisk_create_voicemail_conf($server, $user) {
  $uri = parse_url($server['url']);
  $host =  $uri['host'];
  // XXX to be changed to $serverid$uid
  $voicemail_conf = "{$user->name}-{$host} => {$user->pin}, {$user->name}, {$user->mail}\n";
  return $voicemail_conf;
}

/**
 * Writes the configuration include file (overwriting previous file if it exists)
 * @param $contents
 *   string the contents of the file to be written
 * @param $location
 *   string filesystem path where the file should be written
 */
function asterisk_write_file($contents, $location) {
  $fh = fopen($location, 'w');
  $ret = fwrite($fh, $contents);
  fclose($fh);
  return $ret;
}

/**
 * Return remote server information associated with the specified user.
 *
 * @param
 *   string the URL to the xmlrpc.php file that will process the call in the server
 * @param
 *   integer user id
 * @param
 *   string phone pin
 * @return
 *   array with remote server information or NULL in case of failure. In case of
 *     failure, check asterisk_error_msg() to make sure it wasn't a processing 
 *     error.
 */
function asterisk_retrieve_user_server_info($local_server_url, $uid, $phone_pin) {

  // reset error handler
  asterisk_error_reset();

  $rc = @xmlrpc($local_server_url,
               'asterisk.getUserServerInfo',
               $uid,
               $phone_pin);

global $agi;
$agi->conlog("value returned from getUserServerInfo: " . serialize($rc));

  if (!$rc || (isset($rc->is_error) && $rc->is_error)) {
    // if xmlrpc call fails...
    asterisk_error( "XMLRPC error checking user id and phone pin: " . xmlrpc_error_msg());
$agi->conlog("error message: " . xmlrpc_error_msg());
    return NULL;
  }

  if (!$rc['success']) {
    // invalid user id / phone pin combination
    return NULL;
  }

  $r = $rc['server_info'];

  return $r;
}

/**
 * Check if the specified user id and phone pin match the one of a user in the system
 *
 * @param
 *   string the URL to the xmlrpc.php file that will process the call in the server
 * @param
 *   integer user id
 * @param
 *   string phone pin
 * @return
 *   boolean True in case the user is valid.  Returns False otherwise or in case of error.
 */
function asterisk_check_user($server_url, $uid, $phone_pin) {

  // reset error handler
  asterisk_error_reset();

  $rc = xmlrpc($server_url,
               'asterisk.isUserValid',
               $uid,
               $phone_pin);

  if (!$rc->is_error) {
    return $rc;
  }
  else {
    // if xmlrpc call fails...
    asterisk_error( "XMLRPC error checking user id and phone pin: " . xmlrpc_error_msg());
    return FALSE;
  }
}
?>
