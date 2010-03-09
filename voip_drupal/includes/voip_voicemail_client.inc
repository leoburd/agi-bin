<?php
/* $Id: voip_voicemail_client.inc,v 1.4 2006/11/01 12:03:22 leob Exp $ */

/**
 * @file
 *   Basic functions to handle voicemail boxes and messages.
 */


// -----------------------------------------------------------------------------
// include required files
// -----------------------------------------------------------------------------

foreach (array( 'audio_server_api.inc', 'error_handler.inc', 'voip_error_codes.inc', 'xmlrpc.inc') as $file) {
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
 * Sends a voicemail message to the specified mailbox in the given server
 *
 * @param $voicemail_server
 *   string with the URL of the xmlrpc.php file of the Drupal site where voip_voicemail.module is located.
 * 
 * @param $user_info
 * array with Drupal URL, name, password and voip extension of the user sending the message
 * 
 * @param $destination
 * array with the number of the mailboxes the message is to be sent to
 * 
 * @param $msg_info
 * array with caller_id, audio file name
 * 
 * @return
 *   array with
 *     ['error_code'] :: VOIP_SUCCESS, VOIP_PROCESSING_FAILURE, VOIP_INVALID_USER_INFO, VOIP_LACK_OF_PERMISSION
 *     ['error_msg'] :: string with eventual error message
 */
function voip_voicemail_msg_create($voicemail_server, $user_info, $destination, $msg_info) {
eh_log("voip_voicemail_msg_create($voicemail_server, $user_info, $destination, $msg_info)");
eh_log("user_info: " . serialize($user_info));
eh_log("$msg_info: " . serialize($msg_info));

  $file = $msg_info['audio_file'];
  unset($msg_info['audio_file']);
  
  $result= xmlrpc($voicemail_server, 'voip_voicemail.messageCreate', $user_info, $destination, $msg_info);
  
eh_log("XML-RPC voip_voicemail.messageCreate returned: " . serialize($result));
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
  
  // now prepare to upload the audio content of the message
  $audio_key = $result['audio_key'];
eh_log("audio_key: $audio_key");

  $audio_info['title'] = "Voicemail message from " . $msg_info['caller_id']['name'] . '<' . $msg_info['caller_id']['number'] . '>';
  $audio_info['body'] = '';
  $audio_info['file'] = $file;
  $audio_info['status'] = 0;
  $audio_info['promote'] = FALSE;
  $audio_info['downloadable'] = FALSE;
  $audio_info['categories'] = array();
  $audio_info['date_created'] = time();
  $audio_info['extra_fields'] = array( 'voip_voicemail_audio_key' => $audio_key);

  // add audio part of the message to the queue to be uploaded to the server
  $r = asa_add_audio_entry_to_upload_queue($voicemail_server, $user_info, $audio_info);
eh_log("Value returned by asa_add_audio_entry_to_upload_queue: $r");
  if (!$r) {
    $rc['error_code'] = VOIP_PROCESSING_FAILURE;
    $rc['error_msg'] = "Error adding voicemail message contents to upload queue:" . eh_error_msg();
    return $rc;
  }
  else {
    $rc['error_code'] = VOIP_SUCCESS;
  }
  
  return $result;
}

/*
 * Deletes the specified voicemail message in the given server
 *
 * @param $voicemail_server
 *   string with the URL of the xmlrpc.php file of the Drupal site where voip_voicemail.module is located.
 * 
 * @param $user_info
 * array with Drupal URL, name, password and voip extension of the user sending the message
 * 
 * @param $msg_info
 * array with information about the message to be deleted
 * 
 * @return
 *   array with
 *     ['error_code'] :: VOIP_SUCCESS, VOIP_PROCESSING_FAILURE, VOIP_INVALID_USER_INFO, VOIP_LACK_OF_PERMISSION
 *     ['error_msg'] :: string with eventual error message
 */
function voip_voicemail_msg_delete($voicemail_server, $user_info, $msg_info) {

  $result= xmlrpc($voicemail_server, 'voip_voicemail.messageDelete', $user_info, $msg_info);
  
eh_log("XML-RPC voip_voicemail.messageDelete returned: " . serialize($result));
  if (!$result) {
    eh_error("XMLRPC error:" . xmlrpc_error_msg());
    $rc['error_code'] = VOIP_PROCESSING_FAILURE;
    $rc['error_msg'] = "XMLRPC error:" . xmlrpc_error_msg();
    return $rc;
  }

  return $result;
}

/*
 * Updates the statistics associated with the playing of the specified voicemail message.
 *
 * @param $voicemail_server
 *   string with the URL of the xmlrpc.php file of the Drupal site where voip_voicemail.module is located.
 * 
 * @param $user_info
 * array with Drupal URL, name, password and voip extension of the user performing the operation
 * 
 * @param $msg_info
 * array with information about the voicemail message
 * 
 * @return
 *   array with
 *     ['error_code'] :: VOIP_SUCCESS, VOIP_PROCESSING_FAILURE, VOIP_INVALID_USER_INFO, VOIP_LACK_OF_PERMISSION
 *     ['error_msg'] :: string with eventual error message
 */
function voip_voicemail_msg_save($voicemail_server, $user_info, $msg_info) {

  $result= xmlrpc($voicemail_server, 'voip_voicemail.messageSave', $user_info, $msg_info);
  
eh_log("XML-RPC voip_voicemail.messageSave returned: " . serialize($result));
  if (!$result) {
    eh_error("XMLRPC error:" . xmlrpc_error_msg());
    $rc['error_code'] = VOIP_PROCESSING_FAILURE;
    $rc['error_msg'] = "XMLRPC error:" . xmlrpc_error_msg();
    return $rc;
  }

  return $result;
}



/*
 * Gets voicemail messages from the specified mailbox of the given server
 *
 * @param $voicemail_server
 *   string with the URL of the xmlrpc.php file of the Drupal site where voip_voicemail.module is located.
 * 
 * @param $user_info
 * array with Drupal URL, name, password and voip extension of the user executing the operation
 * 
 * @param $mailbox_number
 * int.
 * 
 * @return
 *   array with
 *     ['error_code'] :: VOIP_SUCCESS, VOIP_VOICEMAIL_MAILBOX_DOES_NOT_EXIST, VOIP_PROCESSING_FAILURE, VOIP_INVALID_USER_INFO, VOIP_LACK_OF_PERMISSION
 *     ['error_msg'] :: string with eventual error message
 *     ['meta_messages'] :: array with information about the messages in the mailbox
 */
function voip_voicemail_mailbox_get_messages($voicemail_server, $user_info, $mailbox_number) {

  $result= xmlrpc($voicemail_server, 'voip_voicemail.mailboxGetMessages', $user_info, $mailbox_number);
  
eh_log("XML-RPC voip_voicemail.mailboxGetMessages returned: " . serialize($result));
  if (!$result) {
    eh_error("XMLRPC error:" . xmlrpc_error_msg());
    $rc['error_code'] = VOIP_PROCESSING_FAILURE;
    $rc['error_msg'] = "XMLRPC error:" . xmlrpc_error_msg();
    return $rc;
  }

  return $result;
}

/*
 * Gets the number of new and old messages in the specified mailbox of the given server
 *
 * @param $voicemail_server
 *   string with the URL of the xmlrpc.php file of the Drupal site where voip_voicemail.module is located.
 * 
 * @param $user_info
 * array with Drupal URL, name, password and voip extension of the user executing the operation
 * 
 * @param $mailbox_number
 * int.
 * 
 * @return
 *   array with
 *     ['error_code'] :: VOIP_SUCCESS, VOIP_PROCESSING_FAILURE, VOIP_INVALID_USER_INFO, VOIP_LACK_OF_PERMISSION
 *     ['error_msg'] :: string with eventual error message
 *     ['count_new_messages'] :: number of new messages in the mailbox
 *     ['count_old_messages'] :: number of old messages in the mailbox
 */
function voip_voicemail_mailbox_get_number_of_messages($voicemail_server, $user_info, $mailbox_number) {

  $result= xmlrpc($voicemail_server, 'voip_voicemail.mailboxGetNumberOfMessages', $user_info, $mailbox_number);
  
eh_log("XML-RPC voip_voicemail.mailboxGetNumberOfMessages returned: " . serialize($result));
  if (!$result) {
    eh_error("XMLRPC error:" . xmlrpc_error_msg());
    $rc['error_code'] = VOIP_PROCESSING_FAILURE;
    $rc['error_msg'] = "XMLRPC error:" . xmlrpc_error_msg();
    return $rc;
  }

  return $result;
}

/*
 * Gets the number of new and old messages in the specified mailbox of the given server
 *
 * @param $voicemail_server
 *   string with the URL of the xmlrpc.php file of the Drupal site where voip_voicemail.module is located.
 * 
 * @param $user_info
 * array with Drupal URL, name, password and voip extension of the user executing the operation
 * 
 * @param $mailbox_number
 * int.
 * 
 * @return
 *   array with
 *     ['error_code'] :: VOIP_SUCCESS, VOIP_PROCESSING_FAILURE, VOIP_INVALID_USER_INFO, VOIP_LACK_OF_PERMISSION
 *     ['error_msg'] :: string with eventual error message
 *     ['mailbox_info'] :: array with mailbox info
 */
function voip_voicemail_mailbox_get_info($voicemail_server, $user_info, $mailbox_number) {
eh_log("Entering: voip_voicemail_mailbox_get_info($voicemail_server, $user_info, $mailbox_number)");
  $result= xmlrpc($voicemail_server, 'voip_voicemail.mailboxGetInfo', $user_info, $mailbox_number);
  
eh_log("XML-RPC voip_voicemail.mailboxGetInfo returned: " . serialize($result));
  if (!$result) {
    eh_error("XMLRPC error:" . xmlrpc_error_msg());
    $rc = array();
    $rc['error_code'] = VOIP_PROCESSING_FAILURE;
    $rc['error_msg'] = "XMLRPC error:" . xmlrpc_error_msg();
    return $rc;
  }

  if ($result['error_code'] != VOIP_SUCCESS) {
    return $result;
  }
  
  // retrieve the audio file associated with the mailbox greetings
    if ($result['mailbox_info']['nid_audio_greetings']) {
    $basename = asa_get_audio_file($voicemail_server, $user_info, array('nid' => $result['mailbox_info']['nid_audio_greetings']));
    if (($basename == -1) || ($basename === 0)) {
      $rc = array();
      $rc['error_code'] = VOIP_PROCESSING_FAILURE;
      $rc['error_msg'] =  "Error retrieving audio name file: " . eh_error_msg();
      return $rc;
    }  
    // create a new entry in extension_info for the file
    $result['mailbox_info']['file_audio_description'] = $basename;
  }
 
  return $result;
}


// -----------------------------------------------------------------------------
// private function definitions
// -----------------------------------------------------------------------------




?>
