<?php
/* $Id: voip_client.inc,v 1.6 2006/11/01 12:03:22 leob Exp $ */

/**
 * @file
 *   Basic functions to handle VoIP extensions and users.
 */


// -----------------------------------------------------------------------------
// include required files
// -----------------------------------------------------------------------------

foreach (array( 'audio_server_api.inc', 'error_handler.inc', 'voip_error_codes.inc', 'voip_cache.inc', 'voip_io.inc', 'common.inc', 'xmlrpc.inc') as $file) {
  require_once($file);
}


// -----------------------------------------------------------------------------
// constants
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// global variables
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// initialization
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// public function definitions
// -----------------------------------------------------------------------------

/*
 * Adds new entry into log table
 */
function voip_log($voip_server, $user_info, $category, $action, $to_extension_number = NULL, $notes = NULL) {
  $result = @xmlrpc($voip_server, 'voip.addLogEntry', $user_info, $category, $action, (int)$to_extension_number, $notes);
  if ($result === FALSE) {
    // if xmlrpc call fails...
    $result['error_code'] = VOIP_PROCESSING_FAILURE;
    $result['error_msg'] =  "XMLRPC error adding entry to call-history table: " . xmlrpc_error_msg();
  }
  return $result;
}


/*
 * Returns Drupal name and password of the user associated with the given extension/phone pin
 *
 * @param $voip_server
 *   string with the URL of the xmlrpc.php file of the Drupal site where voip.module is located.
 * 
 * @return
 *   array with
 *     ['error_code'] :: 1 = success, -1 = processing failure, -2 = invalid extension/password combination
 *     ['error_msg'] :: string with eventual error message
 *     ['user_info'] :: array with 'user_name' and 'password' of the user in the Drupal site pointed by the extension
 */
function voip_user_get_info($voip_server, $extension_number, $phone_pin) {
  $result = @xmlrpc($voip_server, 'voip.userGetInfo', (int)$extension_number, (int)$phone_pin);
  if ($result === FALSE) {
    // if xmlrpc call fails...
    $result['error_code'] = VOIP_PROCESSING_FAILURE;
    $result['error_msg'] =  "XMLRPC error retrieving user info: " . xmlrpc_error_msg();
  }
  
  return $result;
}


/**
 * Delete the specified node voip extension.  This function does not work for user extensions.
 *
 * @param $voip_server
 *   string the URL to the xmlrpc.php file that will process the call in the server
 * 
 * @param $user_info
 * array with Drupal name and password of the user requesting the delete
 * 
 * @param $nid
 *   integer with the node id
 * 
 * @return
 *   array with
 *     ['error_code'] :: 1 = VOIP_SUCCESS, VOIP_PROCESSING_FAILURE, VOIP_INVALID_USER_INFO, VOIP_INVALID_EXTENSION_NUMBER, VOIP_INVALID_NID, VOIP_LACK_OF_PERMISSION
 *     ['error_msg'] :: string with eventual error message
 */
function voip_extension_delete_node_extension($voip_server, $user_info, $extension_info) {
  $rc = array();
  
  if (!isset($extension_info['nid_contents'])) {
    eh_error_msg('Invalid extension being deleted.');
    $rc['error_code'] = VOIP_PROCESSING_FAILURE;
    $rc['error_msg'] = 'Invalid extension being deleted.';
    return $rc;
  }

  $result = @xmlrpc($voip_server, 'voip.extensionDelete', $user_info, (int)$extension_info['nid_contents']);

  if (!$result) {
    // if xmlrpc call fails...
    $rc['error_code'] = VOIP_PROCESSING_FAILURE;
    $rc['error_msg'] =  "XMLRPC error deleting voip extension: " . xmlrpc_error_msg();
    return $rc;
  }

  if ($result['error_code'] !== VOIP_SUCCESS) {
    return $result;
  }
  
  // now delete the audio files associated with the extension's name, greetings and description
  $audio_fields = array('file_audio_name', 'file_audio_greetings', 'file_audio_description');
  foreach($audio_fields as $audio_field) {
    if (isset($extension_info[$audio_field])) {
    unlink($extension_info[$audio_field] . ".wav");
    }
  }

  return $result;
}

/**
 * Return information associated with the specified voip extension.
 *
 * @param $voip_server
 *   string the URL to the xmlrpc.php file that will process the call in the server
 * 
 * @param $user_info
 * array with Drupal name and password of the user requesting the extension info
 * 
 * @param $extension_number
 *   integer with the extension number
 * 
 * @return
 *   array with
 *     ['error_code'] :: VOIP_SUCCESS, VOIP_PROCESSING_FAILURE, VOIP_INVALID_USER_INFO, VOIP_INVALID_EXTENSION_NUMBER, VOIP_LACK_OF_PERMISSION
 *     ['extension_info''] :: array with extension info, NULL in case of invalid extension
 *     ['error_msg'] :: string with eventual error message
 */
function voip_extension_get_info($voip_server, $user_info, $extension_number) {

  $rc = array();

  $result = @xmlrpc($voip_server, 'voip.extensionGetInfo', $user_info, (int)$extension_number);
  if (!$result) {
    // if xmlrpc call fails...
    $rc['error_code'] = VOIP_PROCESSING_FAILURE;
    $rc['error_msg'] =  "XMLRPC error retrieving voip extension info: " . xmlrpc_error_msg();
    return $rc;
  }

  if ($result['error_code'] !== VOIP_SUCCESS) {
    return $result;
  }
  
  // retrieve extension-related files
  $rc = _voip_get_extension_files($voip_server, $user_info, $result['extension_info']);  
  return $rc;
}

/**
 * Return voip extension associated with the specified node id.
 *
 * @param $voip_server
 *   string the URL to the xmlrpc.php file that will process the call in the server
 * 
 * @param $user_info
 * array with Drupal name and password of the user requesting the extension info
 * 
 * @param $nid
 *   integer with the node id
 * 
 * @return
 *   array with
 *     ['error_code'] :: VOIP_SUCCESS, VOIP_PROCESSING_FAILURE, VOIP_INVALID_USER_INFO, VOIP_INVALID_EXTENSION_NUMBER, VOIP_LACK_OF_PERMISSION
 *     ['extension_info''] :: array with extension info, NULL in case of invalid extension
 *     ['error_msg'] :: string with eventual error message
 */
function voip_extension_get_info_from_nid($voip_server, $user_info, $nid) {
  $rc = array();

  $result = @xmlrpc($voip_server, 'voip.extensionGetInfoFromNid', $user_info, (int)$nid);

  if (!$result) {
    // if xmlrpc call fails...
    $rc['error_code'] = VOIP_PROCESSING_FAILURE;
    $rc['error_msg'] =  "XMLRPC error retrieving voip extension info: " . xmlrpc_error_msg();
    return $rc;
  }

  if ($result['error_code'] !== VOIP_SUCCESS) {
    return $result;
  }
  
  // retrieve extension-related files
  $rc = _voip_get_extension_files($voip_server, $user_info, $result['extension_info']);  
  return $rc;
}

/**
 * Return voip extension associated with the specified user id.
 *
 * @param $voip_server
 *   string the URL to the xmlrpc.php file that will process the call in the server
 * 
 * @param $user_info
 * array with Drupal name and password of the user requesting the extension info
 * 
 * @param $uid
 *   integer with the user id
 * 
 * @return
 *   array with
 *     ['error_code'] :: 1 = VOIP_SUCCESS, VOIP_PROCESSING_FAILURE, VOIP_INVALID_USER_INFO, VOIP_LACK_OF_PERMISSION
 *     ['extension_info''] :: array with extension info, NULL in case of invalid extension
 *     ['error_msg'] :: string with eventual error message
 */
function voip_extension_get_info_from_uid($voip_server, $user_info, $uid) {
//eh_log("Entering: voip_extension_get_info_from_uid($voip_server, $user_info, $uid)");

  $rc = array();
  $rc['success'] = TRUE;
  $rc['extension_info'] = NULL;
  $rc['error_msg'] = '';

  $result = @xmlrpc($voip_server, 'voip.extensionGetInfoFromUid', $user_info, $uid);

//eh_log("value returned from extensionGetInfoFromUid: " . serialize($result));

  if (!$result) {
    // if xmlrpc call fails...
    $rc['error_code'] = VOIP_PROCESSING_FAILURE;
    $rc['error_msg'] =  "XMLRPC error retrieving voip extension info: " . xmlrpc_error_msg();
    return $rc;
  }

  if ($result['error_code'] !== VOIP_SUCCESS) {
    return $result;
  }
  
  // retrieve extension-related files
  $rc = _voip_get_extension_files($voip_server, $user_info, $result['extension_info']);  
  return $rc;
}

/**
 * Sets the audio name/description associated with the specified extension.
 *
 * @param $voip_server
 *   string the URL to the xmlrpc.php file that will process the call in the server
 * 
 * @param $user_info
 * array with Drupal name and password of the user requesting the extension info
 * 
 * @param $extension_number
 *   integer with the extension number
 * 
 * @param $field_name
 * string 'audio_name', 'audio_description', 'audio_greetings'
 * 
 * @param $file_name
 * string with the name of the audio file to be used
 * 
 * @return
 *   array with
 *     ['error_code'] :: VOIP_SUCCESS, VOIP_PROCESSING_FAILURE, VOIP_INVALID_USER_INFO, VOIP_INVALID_EXTENSION_NUMBER, VOIP_LACK_OF_PERMISSION
 *     ['error_msg'] :: string with eventual error message
 * 
 *   Note: it may take a while for the extension to be updated with the new contents. the calling module is responsibe for checking and retrieving the updated extension info.
 */
function voip_extension_set_audio_field($voip_server, $user_info, $extension_number, $field_name, $file_name) {
  $result= xmlrpc($voip_server, 'voip.extensionSetAudioField', $user_info, $extension_number, $field_name);
  
  if (!$result) {
    eh_error("XMLRPC error:" . xmlrpc_error_msg());
    $rc['error_code'] = VOIP_PROCESSING_FAILURE;
    $rc['error_msg'] = "XMLRPC error:" . xmlrpc_error_msg();
    return $rc;
  }

  if ($result['error_code'] !== VOIP_SUCCESS) {
    eh_error("Processing error sending voicemail message: {$result['error_msg']}");
    return $result;
  }
  
  // now prepare to upload the contents of the audio file to the appropriate extension field
  $audio_key = $result['audio_key'];
  
  if ($field_name == 'audio_name') {
    $extra_fields = array( 'voip_audio_key_name' => $audio_key);
  }
  elseif ($field_name == 'audio_greetings') {
    $extra_fields = array( 'voip_audio_key_greetings' => $audio_key);
  }
  else {
    $extra_fields = array( 'voip_audio_key_description' => $audio_key);
  }
  
  $audio_info['title'] = "Audio contents for field $field_name of voip extension $extension_number" ;
  $audio_info['body'] = '';
  $audio_info['file'] = $file_name;
  $audio_info['status'] = 0;
  $audio_info['promote'] = FALSE;
  $audio_info['downloadable'] = FALSE;
  $audio_info['categories'] = array();
  $audio_info['date_created'] = time();
  $audio_info['extra_fields'] = $extra_fields;

  // add audio part of the message to the queue to be uploaded to the server
  $r = asa_add_audio_entry_to_upload_queue($voip_server, $user_info, $audio_info);
  if (!$r) {
    $rc['error_code'] = VOIP_PROCESSING_FAILURE;
    $rc['error_msg'] = "Error adding voip extension contents to upload queue:" . eh_error_msg();
    return $rc;

  // Now update extension to be returned
  $rc['error_code'] = VOIP_SUCCESS;
  // Note: it may take a couple of minutes the extension to be actually updated with the audio contents
  }
  
  return $result;
}

/**
 * Sets the audio name/description associated with the specified extension.
 *
 * @param $voip_server
 *   string the URL to the xmlrpc.php file that will process the call in the server
 * 
 * @param $user_info
 * array with Drupal name and password of the user requesting the extension info
 * 
 * @param $extension_number
 *   integer with the extension number
 * 
 * @param $category_ids
 * array with the ids of the categories that will be associated with the extension
 * 
 * @return
 *   array with
 *     ['error_code'] :: VOIP_SUCCESS, VOIP_PROCESSING_FAILURE, VOIP_INVALID_USER_INFO, VOIP_INVALID_EXTENSION_NUMBER, VOIP_LACK_OF_PERMISSION
 *     ['error_msg'] :: string with eventual error message
 */
function voip_extension_set_categories($voip_server, $user_info, $extension_number, $category_ids) {
  $result= xmlrpc($voip_server, 'voip.extensionSetCategories', $user_info, (int)$extension_number, $category_ids);
  
  if (!$result) {
    eh_error("XMLRPC error:" . xmlrpc_error_msg());
    $rc['error_code'] = VOIP_PROCESSING_FAILURE;
    $rc['error_msg'] = "XMLRPC error:" . xmlrpc_error_msg();
    return $rc;
  }

  if ($result['error_code'] !== VOIP_SUCCESS) {
    eh_error("Processing error setting extension categories: {$result['error_msg']}");
  }
  
  return $result;
}

/**
 * Sets the audio name/description associated with the specified extension.
 *
 * @param $voip_server
 *   string the URL to the xmlrpc.php file that will process the call in the server
 * 
 * @param $user_info
 *   array with Drupal name and password of the user requesting the extension info
 * 
 * @param $old_pin
 *   int with current phone pin of the user
 * 
 * @param $new_pin
 *   int with the new phone pin
 * 
 * @return
 *   array with
 *     ['error_code'] :: VOIP_SUCCESS, VOIP_PROCESSING_FAILURE, VOIP_INVALID_USER_INFO, VOIP_INVALID_PHONE_PIN
 *     ['error_msg'] :: string with eventual error message
 */
function voip_change_phone_pin($voip_server, $user_info, $old_pin, $new_pin) {
eh_log("Entering: voip_change_phone_pin($voip_server, $user_info, $old_pin, $new_pin)");
  $result= xmlrpc($voip_server, 'voip.changePhonePin', $user_info, (int)$old_pin, (int)$new_pin);
  
eh_log("XML-RPC voip.changePhonePin returned: " . serialize($result));
  if (!$result) {
    eh_error("XMLRPC error:" . xmlrpc_error_msg());
    $rc['error_code'] = VOIP_PROCESSING_FAILURE;
    $rc['error_msg'] = "XMLRPC error:" . xmlrpc_error_msg();
    return $rc;
  }

  if ($result['error_code'] !== VOIP_SUCCESS) {
    eh_error("Processing error changing user's phone pin: {$result['error_msg']}");
  }
  
  return $result;
}

/**
 * Sets the system voice to be used by the specified user extension.
 *
 * @param $voip_server
 *   string the URL to the xmlrpc.php file that will process the call in the server
 * 
 * @param $user_info
 *   array with Drupal name and password of the user requesting the extension info
 * 
 * @param $voice_id
 *   int the id of the voice to be used
 * 
 * @return
 *   array with
 *     ['error_code'] :: VOIP_SUCCESS, VOIP_PROCESSING_FAILURE, VOIP_INVALID_USER_INFO, VOIP_INVALID_PHONE_PIN
 *     ['error_msg'] :: string with eventual error message
 */
function voip_change_voice_id($voip_server, $user_info, $voice_id) {
  $result= xmlrpc($voip_server, 'voip.changeVoiceId', $user_info, (int)$voice_id);
  
  if (!$result) {
    eh_error("XMLRPC error:" . xmlrpc_error_msg());
    $rc['error_code'] = VOIP_PROCESSING_FAILURE;
    $rc['error_msg'] = "XMLRPC error:" . xmlrpc_error_msg();
    return $rc;
  }

  if ($result['error_code'] !== VOIP_SUCCESS) {
    eh_error("Processing error changing user's voice id: {$result['error_msg']}");
  }
  
  return $result;
}


/*
 * Retrieve list of categories available for voip extensions
 *
 * @param $voip_server
 *   string the URL to the xmlrpc.php file that will process the call in the server
 *
 * @return
 *   mixed value. In case of success, returns array with categories associated with
 *   voip extensions.  In case of failure, returns FALSE; check eh_error_msg() for explanation.
 */
function voip_get_category_list($voip_server) {

  $rc = @xmlrpc($voip_server, 'voip.getCategoryList');

  if ($rc !== FALSE) {       // if xmlrpc call successful...
      return $rc;     // return an array with the categories
  }
  else {
    eh_error("XMLRPC error retrieving voip category list: " . xmlrpc_error_msg());
    return FALSE;
  }
}

function voip_get_extension_directory($voip_server, $user_info, $query_info, $start, $limit) {

  $rc = @xmlrpc($voip_server, 'voip.getDirectory', $user_info, $query_info, (int)$start, (int)$limit);
  if ($rc === FALSE) { // xmlrpc error
    eh_error("XMLRPC error retrieving audio entries: " . xmlrpc_error_msg());
    $r = array();
    $r['error_code'] = VOIP_PROCESSING_FAILURE;
    return $r;
  }

  if ($rc['error_code'] != VOIP_SUCCESS) {
    eh_error("Error retrieving audio entries: {$rc['error_msg']}");
    return $rc;
  }
  
  // now retrieve the audio files associated with each one of the extensions returned
  foreach($rc['extensions'] as $index => $extension_info) {
    // retrieve extension-related files
    $r = _voip_get_extension_files($voip_server, $user_info, $extension_info);  
    if ($r['error_code'] != VOIP_SUCCESS) {
      return $r;
    }
    else {
      $rc['extensions'][$index] = $r['extension_info'];
    }
  }

  return $rc;
}

/*
 * Recycle extensions that have not been used and update cache with extensions that have recently been modified
 */
function voip_recycle_extensions($voip_server, $user_info, $last_modified_period = 1, $last_accessed_period = 10) {
  // retrieve extensions that have been modified since $last_modified_period days
  $start_date = time() - (24*60*60*$last_modified_period);
  $rc = _voip_get_recent_extensions($voip_server, $user_info, $start_date);
  // delete from cache voip_extensions that have not been used for more than $last_accessed_period days
  vc_recycle($last_accessed_period); 
}



// -----------------------------------------------------------------------------
// private function definitions
// -----------------------------------------------------------------------------

function _voip_get_recent_extensions($voip_server, $user_info, $start_date, $limit = 0) {
  $rc = xmlrpc($voip_server, 'voip.getRecentExtensions', $user_info, (int)$start_date, (int)$limit);
  if ($rc === FALSE) { // xmlrpc error
    eh_error("XMLRPC error retrieving audio entries: " . xmlrpc_error_msg());
    $r = array();
    $r['error_code'] = VOIP_PROCESSING_FAILURE;
    return $r;
  }

  if ($rc['error_code'] != VOIP_SUCCESS) {
    eh_error("Error retrieving recent extensions: {$rc['error_msg']}");
    return $rc;
  }
  
  // now retrieve the audio files associated with each one of the extensions returned
  foreach($rc['extensions'] as $index => $extension_info) {
    // retrieve extension-related files
    $r = _voip_get_extension_files($voip_server, $user_info, $extension_info);  
    if ($r['error_code'] != VOIP_SUCCESS) {
      return $r;
    }
    else {
      $rc['extensions'][$index] = $r['extension_info'];
    }
  }

  return $rc;
}


function _voip_get_extension_files($voip_server, $user_info, $extension_info) {

  $fields = array('audio_name', 'audio_greetings', 'audio_description');
  $rc = array();

  // check if the extension is already in cache
  $r = vc_retrieve_extension_by_number($voip_server, $extension_info['number']);
  if ($r) {
    if ($r == -1) {
      $rc['error_code'] = VOIP_PROCESSING_FAILURE;
      $rc['error_msg'] =  "Error retrieving voip extension info from cache";
    }
    else {
      if ($extension_info['last_modified'] <= $r['last_modified']) {
        $rc['error_code'] = VOIP_SUCCESS;
        $rc['extension_info'] = $r;
        // update access flags for each of the audio nodes retrieved
        foreach ($fields as $field) {
          if ($extension_info["nid_$field"]) {
            asa_touch_node($voip_server, $extension_info["nid_$field"]);
          }
        }
      }
      else {
        vc_delete_entry($voip_server, $extension_info['number']);
      }
    }
  }
 
  if (empty($rc)) {      
    // retrieve the audio files associated with each one of the extensions returned
    foreach ($fields as $field) {
      if ($extension_info["nid_$field"]) {
        $basename = asa_get_audio_file($voip_server, $user_info, array('nid' => $extension_info["nid_$field"]));
        if (($basename == -1) || ($basename === 0)) {
          $rc['error_code'] = VOIP_PROCESSING_FAILURE;
          $rc['error_msg'] =  "Error retrieving audio name file: " . eh_error_msg();
          return $rc;
        }  
        // create a new entry in extension_info for the file
        $extension_info["file_$field"] = $basename;
      }
    }
    // store extension in cache
    vc_store_extension($voip_server, $extension_info);
    $rc['error_code'] = VOIP_SUCCESS;
    $rc['extension_info'] = $extension_info;
  }
  return $rc;
}

?>
