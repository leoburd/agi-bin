<?php
/* $Id: voip_group_client.inc,v 1.4 2006/11/01 12:03:22 leob Exp $ */

/**
 * @file
 *   Basic functions to handle voicemail groups.
 */


// -----------------------------------------------------------------------------
// include required files
// -----------------------------------------------------------------------------

foreach (array( 'error_handler.inc', 'og_xmlrpc.inc', 'voip_client.inc', 'xmlrpc.inc') as $file) {
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
 * Create a voip group. 
 * 
 * NOTE: It may take a while for audio files to be uploaded.  The caller module has to keep checking extension updates from time to time.
 */
function voip_group_create_group($xmlrpc_server, $user_info, $group_info, $file_audio_name = NULL, $file_audio_description = NULL) {
eh_log("voip_group_create_group($xmlrpc_server, $user_info, $group_info, $file_audio_name)");
eh_log("user_info: " . serialize($user_info));
eh_log("group_info: " . serialize($group_info));

  $rc = array('error_code' => VOIP_SUCCESS);

  // create the basic voip group  
  $result = xmlrpc($xmlrpc_server, 'voip_group.createGroup', $user_info, $group_info);
eh_log("XML-RPC voip_group.createGroup returned: " . serialize($result));
  if (!$result) {
    eh_error("XMLRPC error:" . xmlrpc_error_msg());
    $rc['error_code'] = VOIP_PROCESSING_FAILURE;
    $rc['error_msg'] = "XMLRPC error:" . xmlrpc_error_msg();
    return $rc;
  }

  if ($result['error_code'] !== VOIP_SUCCESS) {
    eh_error("Error creating voip_group: {$result['error_msg']}");
    return $result;
  }

  // Now upload the audio name associated with the group
  $extension_info = $result['extension_info'];
  if ($file_audio_name) {
    $rc = voip_extension_set_audio_field($xmlrpc_server, $user_info, $extension_info['number'], 'audio_name', $file_audio_name);
     if ($rc['error_code'] !== VOIP_SUCCESS) {
       return $rc;
     }
  }

  // Upload the audio description associated with the group
  if ($file_audio_description) {
    $rc = voip_extension_set_audio_field($xmlrpc_server, $user_info, $extension_info['number'], 'audio_description', $file_audio_description);
     if ($rc['error_code'] !== VOIP_SUCCESS) {
       return $rc;
     }
  }

  return $result;
}

/*
 * Delete a voip group. 
 */
function voip_group_delete_group($xmlrpc_server, $user_info, $extension_info) {
eh_log("voip_group_delete_group($xmlrpc_server, $user_info, $extension_info)");
eh_log("user_info: " . serialize($user_info));
eh_log("extension_info: " . serialize($extension_info));

// TODO: move this function to voip_client.inc and generalize it for any kind of extension...

  $rc = array();
  
  $user_name = isset($user_info['user_name'])?$user_info['user_name']:NULL;
  $password = isset($user_info['password'])?$user_info['password']:NULL;
  
  $result = xmlrpc($xmlrpc_server, 'og.deleteGroup', $user_name, $password, (int)$extension_info['nid_contents']);
eh_log("XML-RPC og.deleteGroup returned: " . serialize($result));
  if (!$result) {
    eh_error("XMLRPC error:" . xmlrpc_error_msg());
    $rc['error_code'] = VOIP_PROCESSING_FAILURE;
    $rc['error_msg'] = "XMLRPC error:" . xmlrpc_error_msg();
    return $rc;
  }

  switch ($result['error_code']) {
    case OG_XMLRPC_SUCCESS:
      $rc['error_code'] = VOIP_SUCCESS;
      $audio_fields = array('file_audio_name', 'file_audio_greetings', 'file_audio_description');
      foreach($audio_fields as $audio_field) {
        if (isset($extension_info[$audio_field])) {
eh_log("voip_group_delete_group :: deleting file: " . $extension_info[$audio_field]);
          unlink($extension_info[$audio_field] . ".wav");
        }
      }
      break;
      
    case OG_XMLRPC_NOT_ALLOWED;
      $rc['error_code'] = VOIP_LACK_OF_PERMISSION;
      $rc['error_msg'] = $result['error_msg'];
      break;

    case OG_XMLRPC_INVALID_USER_NAME_OR_PASSWORD:
      $rc['error_code'] = VOIP_INVALID_USER_INFO;
      $rc['error_msg'] = $result['error_msg'];
      break;
      
    default:
      $rc['error_code'] = VOIP_PROCESSING_FAILURE;
      $rc['error_msg'] = 'Invalid result from og.deleteGroup: ' . serialize($result);
      break;
  }
  
  return $rc;
}

/*
 * Retrieve the group subscriptions for a given user.
 */
function voip_group_get_user_groups($xmlrpc_server, $user_info, $uid) {
eh_log("voip_group_get_user_groups($xmlrpc_server, $user_info, $uid)");
eh_log("user_info: " . serialize($user_info));

  $rc = array();
  
  $user_name = isset($user_info['user_name'])?$user_info['user_name']:NULL;
  $password = isset($user_info['password'])?$user_info['password']:NULL;
  
  $result = xmlrpc($xmlrpc_server, 'og.getUserGroups', $user_name, $password, (int)$uid);
eh_log("XML-RPC og.getUserGroups returned: " . print_r($result, TRUE));
  if (!$result) {
    eh_error("XMLRPC error:" . xmlrpc_error_msg());
    $rc['error_code'] = VOIP_PROCESSING_FAILURE;
    $rc['error_msg'] = "XMLRPC error:" . xmlrpc_error_msg();
    return $rc;
  }

  if ($result['error_code'] !== OG_XMLRPC_SUCCESS) {
    eh_error("Error retrieving group subscriptions: {$result['error_msg']}");
    $rc['error_code'] = VOIP_PROCESSING_FAILURE;
    $rc['error_msg'] = "voip_group error:" . $result['error_msg'];
    return $rc;
  }

  // Now filter-out all non voip_group groups
  $subscriptions = array();
  foreach($result['subscriptions'] as $group) {
  	// $group contains the following fields: title, nid, og_role, is_active (always TRUE), is_admin, uid, mail_type
//$xmlrpc_server .= '?XDEBUG_SESSION_START=whatsup';
  	$r = voip_extension_get_info_from_nid($xmlrpc_server, $user_info, $group['nid']);
eh_log('subscribed to: nid: ' . $group['nid'] . ', extension: ' . print_r($r, TRUE));
  	if ($r['error_code'] == VOIP_SUCCESS) {
  	  if ($r['extension_info']['type_contents'] == 'voip_group') {
  	    $subscriptions[] = array('is_admin' => $group['is_admin'], 'extension_info' => $r['extension_info']);
  	  }
  	}
  	else {
  	  // error retrieving info associated with subscribed group extension
  	  return $r;
  	}
  }
  $rc['error_code'] = VOIP_SUCCESS;
  $rc['subscriptions'] = $subscriptions;
 
  return $rc;
}

/*
 * Get subscribers of the specified group. Returns an array indexed by the extension number of the group subscribers
 */
function voip_group_get_all_subscribers($xmlrpc_server, $user_info, $gid, $min_is_active = 0, $min_is_admin = 0) {
//eh_log("voip_group_get_subscribers($xmlrpc_server, $user_info, $gid, $min_is_active, $min_is_admin)");
//eh_log("user_info: " . serialize($user_info));

  $rc = array();
  
  $user_name = isset($user_info['user_name'])?$user_info['user_name']:NULL;
  $password = isset($user_info['password'])?$user_info['password']:NULL;
  $result = xmlrpc($xmlrpc_server, 'og.getAllSubscribers', $user_name, $password, (int)$gid, $min_is_active, $min_is_admin);
eh_log("XML-RPC og.getAllSubscribers returned: " . print_r($result, TRUE));
  if (!$result) {
    eh_error("XMLRPC error:" . xmlrpc_error_msg());
    $rc['error_code'] = VOIP_PROCESSING_FAILURE;
    $rc['error_msg'] = "XMLRPC error:" . xmlrpc_error_msg();
    return $rc;
  }

  switch ($result['error_code']) {
    case OG_XMLRPC_SUCCESS: // success
      $subscribers = array();
      // now retrieve the extension info associated with each one of the subscribers
      foreach($result['subscribers'] as $subscriber) {
  	    // $subscriber contains the following fields: uid, name, mail, picture, nid, og_role, is_active, is_admin, mail_type
  	    $r = voip_extension_get_info_from_uid($xmlrpc_server, $user_info, $subscriber['uid']);
//eh_log('user extension: ' . serialize($r));
  	    if ($r['error_code'] == VOIP_SUCCESS) {
  	      $extension_info = $r['extension_info'];
  	      $subscribers[$extension_info['number']] = array('is_active' => $subscriber['is_active'], 'is_admin' => $subscriber['is_admin'], 'extension_info' => $extension_info);
  	    }
      }
      $rc['error_code'] = VOIP_SUCCESS;
      $rc['subscribers'] = $subscribers;
      break;
      
    case OG_XMLRPC_NOT_A_MEMBER;
      $rc['error_code'] = VOIP_LACK_OF_PERMISSION;
      $rc['error_msg'] = $result['error_msg'];
      break;

    case OG_XMLRPC_INVALID_USER_NAME_OR_PASSWORD:
      $rc['error_code'] = VOIP_INVALID_USER_INFO;
      $rc['error_msg'] = $result['error_msg'];
      break;
      
    default:
      $rc['error_code'] = VOIP_PROCESSING_FAILURE;
      $rc['error_msg'] = 'Invalid result from og.getAllSubscribers: ' . serialize($result);
      break;
  }
  return $rc;
}

/*
 * Subscribe user to the specified group. 
 */
function voip_group_subscribe($xmlrpc_server, $user_info, $gid, $uid) {
eh_log("voip_group_subscribe($xmlrpc_server, $user_info, $gid, $uid)");
eh_log("user_info: " . serialize($user_info));

  $rc = array();
  
  $user_name = isset($user_info['user_name'])?$user_info['user_name']:NULL;
  $password = isset($user_info['password'])?$user_info['password']:NULL;
  $result = xmlrpc($xmlrpc_server, 'og.subscribeUser', $user_name, $password, (int)$gid, (int)$uid);
eh_log("XML-RPC og.subscribeUser returned: " . serialize($result));
  if (!$result) {
    eh_error("XMLRPC error:" . xmlrpc_error_msg());
    $rc['error_code'] = VOIP_PROCESSING_FAILURE;
    $rc['error_msg'] = "XMLRPC error:" . xmlrpc_error_msg();
    return $rc;
  }

  switch ($result['error_code']) {
    case OG_XMLRPC_SUCCESS:
      $rc['error_code'] = VOIP_SUCCESS;
      $rc['subscription_status'] = $result['subscription_status'];
      break;
      
    case OG_XMLRPC_NOT_ALLOWED;
      $rc['error_code'] = VOIP_LACK_OF_PERMISSION;
      $rc['error_msg'] = $result['error_msg'];
      break;

    case OG_XMLRPC_INVALID_USER_NAME_OR_PASSWORD:
      $rc['error_code'] = VOIP_INVALID_USER_INFO;
      $rc['error_msg'] = $result['error_msg'];
      break;
      
    case OG_XMLRPC_INVALID_UID:
    default:
      $rc['error_code'] = VOIP_PROCESSING_FAILURE;
      $rc['error_msg'] = 'Invalid result from og.subscribeUser: ' . serialize($result);
      break;
  }
  return $rc;
}

/*
 * Unsubscribe user from the specified group. 
 */
function voip_group_unsubscribe($xmlrpc_server, $user_info, $gid, $uid) {
eh_log("voip_group_unsubscribe($xmlrpc_server, $user_info, $gid, $uid)");
eh_log("user_info: " . serialize($user_info));

  $rc = array();
  
  $user_name = isset($user_info['user_name'])?$user_info['user_name']:NULL;
  $password = isset($user_info['password'])?$user_info['password']:NULL;
  $result = xmlrpc($xmlrpc_server, 'og.unsubscribeUser', $user_name, $password, (int)$gid, (int)$uid);
eh_log("XML-RPC og.unsubscribeUser returned: " . serialize($result));
  if (!$result) {
    eh_error("XMLRPC error:" . xmlrpc_error_msg());
    $rc['error_code'] = VOIP_PROCESSING_FAILURE;
    $rc['error_msg'] = "XMLRPC error:" . xmlrpc_error_msg();
    return $rc;
  }

  switch ($result['error_code']) {
    case OG_XMLRPC_SUCCESS:
      $rc['error_code'] = VOIP_SUCCESS;
      break;
      
    case OG_XMLRPC_NOT_ALLOWED;
      $rc['error_code'] = VOIP_LACK_OF_PERMISSION;
      $rc['error_msg'] = $result['error_msg'];
      break;

    case OG_XMLRPC_INVALID_USER_NAME_OR_PASSWORD:
      $rc['error_code'] = VOIP_INVALID_USER_INFO;
      $rc['error_msg'] = $result['error_msg'];
      break;
      
    case OG_XMLRPC_INVALID_UID:
    default:
      $rc['error_code'] = VOIP_PROCESSING_FAILURE;
      $rc['error_msg'] = 'Invalid result from og.unsubscribeUser: ' . serialize($result);
      break;
  }
  return $rc;
}

/*
 * Promote user to administrator of the specified group. 
 */
function voip_group_create_admin($xmlrpc_server, $user_info, $gid, $uid) {
eh_log("voip_group_create_admin($xmlrpc_server, $user_info, $gid, $uid)");
eh_log("user_info: " . serialize($user_info));

  $rc = array();
  
  $user_name = isset($user_info['user_name'])?$user_info['user_name']:NULL;
  $password = isset($user_info['password'])?$user_info['password']:NULL;
  $result = xmlrpc($xmlrpc_server, 'og.createAdmin', $user_name, $password, (int)$gid, (int)$uid);
eh_log("XML-RPC og.createAdmin returned: " . serialize($result));
  if (!$result) {
    eh_error("XMLRPC error:" . xmlrpc_error_msg());
    $rc['error_code'] = VOIP_PROCESSING_FAILURE;
    $rc['error_msg'] = "XMLRPC error:" . xmlrpc_error_msg();
    return $rc;
  }

  switch ($result['error_code']) {
    case OG_XMLRPC_SUCCESS:
      $rc['error_code'] = VOIP_SUCCESS;
      break;
      
    case OG_XMLRPC_NOT_ALLOWED;
      $rc['error_code'] = VOIP_LACK_OF_PERMISSION;
      $rc['error_msg'] = $result['error_msg'];
      break;

    case OG_XMLRPC_INVALID_USER_NAME_OR_PASSWORD:
      $rc['error_code'] = VOIP_INVALID_USER_INFO;
      $rc['error_msg'] = $result['error_msg'];
      break;
      
    case OG_XMLRPC_NOT_A_MEMBER:
    case OG_XMLRPC_INVALID_UID:
    default:
      $rc['error_code'] = VOIP_PROCESSING_FAILURE;
      $rc['error_msg'] = 'Invalid result from og.createAdmin: ' . serialize($result);
      break;
  }
  return $rc;
}

/*
 * Convert administrator back as a regular user of the specified group. 
 */
function voip_group_delete_admin($xmlrpc_server, $user_info, $gid, $uid) {
eh_log("voip_group_delete_admin($xmlrpc_server, $user_info, $gid, $uid)");
eh_log("user_info: " . serialize($user_info));

  $rc = array();
  
  $user_name = isset($user_info['user_name'])?$user_info['user_name']:NULL;
  $password = isset($user_info['password'])?$user_info['password']:NULL;
  $result = xmlrpc($xmlrpc_server, 'og.deleteAdmin', $user_name, $password, (int)$gid, (int)$uid);
eh_log("XML-RPC og.deleteAdmin returned: " . serialize($result));
  if (!$result) {
    eh_error("XMLRPC error:" . xmlrpc_error_msg());
    $rc['error_code'] = VOIP_PROCESSING_FAILURE;
    $rc['error_msg'] = "XMLRPC error:" . xmlrpc_error_msg();
    return $rc;
  }

  switch ($result['error_code']) {
    case OG_XMLRPC_SUCCESS:
      $rc['error_code'] = VOIP_SUCCESS;
      break;
      
    case OG_XMLRPC_NOT_ALLOWED;
      $rc['error_code'] = VOIP_LACK_OF_PERMISSION;
      $rc['error_msg'] = $result['error_msg'];
      break;

    case OG_XMLRPC_INVALID_USER_NAME_OR_PASSWORD:
      $rc['error_code'] = VOIP_INVALID_USER_INFO;
      $rc['error_msg'] = $result['error_msg'];
      break;
      
    case OG_XMLRPC_NOT_A_MEMBER:
    case OG_XMLRPC_INVALID_UID:
    default:
      $rc['error_code'] = VOIP_PROCESSING_FAILURE;
      $rc['error_msg'] = 'Invalid result from og.deleteAdmin: ' . serialize($result);
      break;
  }
  return $rc;
}

/*
 * Approve pending user subscription for the specified group. 
 */
function voip_group_approve($xmlrpc_server, $user_info, $gid, $uid) {
eh_log("voip_group_approve($xmlrpc_server, $user_info, $gid, $uid)");
eh_log("user_info: " . serialize($user_info));

  $rc = array();
  
  $user_name = isset($user_info['user_name'])?$user_info['user_name']:NULL;
  $password = isset($user_info['password'])?$user_info['password']:NULL;
  $result = xmlrpc($xmlrpc_server, 'og.approveUser', $user_name, $password, (int)$gid, (int)$uid);
eh_log("XML-RPC og.approveUser returned: " . serialize($result));
  if (!$result) {
    eh_error("XMLRPC error:" . xmlrpc_error_msg());
    $rc['error_code'] = VOIP_PROCESSING_FAILURE;
    $rc['error_msg'] = "XMLRPC error:" . xmlrpc_error_msg();
    return $rc;
  }

  switch ($result['error_code']) {
    case OG_XMLRPC_SUCCESS:
      $rc['error_code'] = VOIP_SUCCESS;
      break;
      
    case OG_XMLRPC_NOT_ALLOWED;
      $rc['error_code'] = VOIP_LACK_OF_PERMISSION;
      $rc['error_msg'] = $result['error_msg'];
      break;

    case OG_XMLRPC_INVALID_USER_NAME_OR_PASSWORD:
      $rc['error_code'] = VOIP_INVALID_USER_INFO;
      $rc['error_msg'] = $result['error_msg'];
      break;
      
    case OG_XMLRPC_NOT_A_MEMBER:
    case OG_XMLRPC_INVALID_UID:
    default:
      $rc['error_code'] = VOIP_PROCESSING_FAILURE;
      $rc['error_msg'] = 'Invalid result from og.approveUser: ' . serialize($result);
      break;
  }
  return $rc;
}

/*
 * Approve pending user subscription for the specified group. 
 */
function voip_group_deny($xmlrpc_server, $user_info, $gid, $uid) {
eh_log("voip_group_deny($xmlrpc_server, $user_info, $gid, $uid)");
eh_log("user_info: " . serialize($user_info));

  $rc = array();
  
  $user_name = isset($user_info['user_name'])?$user_info['user_name']:NULL;
  $password = isset($user_info['password'])?$user_info['password']:NULL;
  $result = xmlrpc($xmlrpc_server, 'og.denyUser', $user_name, $password, (int)$gid, (int)$uid);
eh_log("XML-RPC og.denyUser returned: " . serialize($result));
  if (!$result) {
    eh_error("XMLRPC error:" . xmlrpc_error_msg());
    $rc['error_code'] = VOIP_PROCESSING_FAILURE;
    $rc['error_msg'] = "XMLRPC error:" . xmlrpc_error_msg();
    return $rc;
  }

  switch ($result['error_code']) {
    case OG_XMLRPC_SUCCESS:
      $rc['error_code'] = VOIP_SUCCESS;
      break;
      
    case OG_XMLRPC_NOT_ALLOWED;
      $rc['error_code'] = VOIP_LACK_OF_PERMISSION;
      $rc['error_msg'] = $result['error_msg'];
      break;

    case OG_XMLRPC_INVALID_USER_NAME_OR_PASSWORD:
      $rc['error_code'] = VOIP_INVALID_USER_INFO;
      $rc['error_msg'] = $result['error_msg'];
      break;
      
    case OG_XMLRPC_NOT_A_MEMBER:
    case OG_XMLRPC_INVALID_UID:
    default:
      $rc['error_code'] = VOIP_PROCESSING_FAILURE;
      $rc['error_msg'] = 'Invalid result from og.denyUser: ' . serialize($result);
      break;
  }
  return $rc;
}

/*
 * Retrieve group subscription mode. 
 */
function voip_group_get_subscription_mode($xmlrpc_server, $gid) {
eh_log("voip_group_get_subscription_mode($xmlrpc_server, $gid)");

  $rc = array();
  $result = xmlrpc($xmlrpc_server, 'og.getSubscriptionMode', (int)$gid);
eh_log("XML-RPC og.getSubscriptionMode returned: " . serialize($result));
  if (!$result) {
    eh_error("XMLRPC error:" . xmlrpc_error_msg());
    $rc['error_code'] = VOIP_PROCESSING_FAILURE;
    $rc['error_msg'] = "XMLRPC error:" . xmlrpc_error_msg();
    return $rc;
  }
  if ($result['error_code'] == OG_XMLRPC_SUCCESS) {
    $rc['error_code'] = VOIP_SUCCESS;
    $rc['subscription_mode'] = $result['subscription_mode'];
  }
  else {
  	$rc['error_code'] = VOIP_PROCESSING_FAILURE;
    $rc['error_msg'] = 'Processing failure. og.getSubscriptionMode returned: ' . serialize($result);
  }
  return $rc;
}

/*
 * Get subscribers of the specified group.
 */
function voip_group_get_subscribers($xmlrpc_server, $user_info, $gid, $min_is_active = 0, $min_is_admin = 0) {
//eh_log("voip_group_get_subscribers($xmlrpc_server, $user_info, $gid, $min_is_active, $min_is_admin)");
//eh_log("user_info: " . serialize($user_info));

  $rc = array();
  $result = xmlrpc($xmlrpc_server, 'voip_group.getSubscribers', $user_info, (int)$gid, (int)$min_is_active, (int)$min_is_admin);
eh_log("XML-RPC voip_group.getSubscribers returned: " . print_r($result, TRUE));
  if (!$result) {
    eh_error("XMLRPC error:" . xmlrpc_error_msg());
    $rc['error_code'] = VOIP_PROCESSING_FAILURE;
    $rc['error_msg'] = "XMLRPC error:" . xmlrpc_error_msg();
    return $rc;
  }
  return $result;
}

// -----------------------------------------------------------------------------
// private function definitions
// -----------------------------------------------------------------------------


?>
