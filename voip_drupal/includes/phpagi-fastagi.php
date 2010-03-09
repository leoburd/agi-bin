#!/usr/bin/php -q
<?php
 /**
  * phpagi-fastagi.php : PHP FastAGI bootstrap
  * Website: http://phpagi.sourceforge.net
  *
  * $Id: phpagi-fastagi.php,v 1.2 2006/09/07 17:58:47 leob Exp $
  *
  * Copyright (c) 2004, 2005 Matthew Asham <matthewa@bcwireless.net>, David Eder <david@eder.us>
  * All Rights Reserved.
  *
  * This software is released under the terms of the GNU Lesser General Public License v2.1
  * A copy of which is available from http://www.gnu.org/copyleft/lesser.html
  *
  * We would be happy to list your phpagi based application on the phpagi
  * website.  Drop me an Email if you'd like us to list your program.
  *
  * @package phpAGI
  * @version 2.0
  * @example docs/fastagi.xinetd Example xinetd config file
  */

 /**
  * Written for PHP 4.3.4, should work with older PHP 4.x versions.
  * Please submit bug reports, patches, etc to http://sourceforge.net/projects/phpagi/
  * Gracias. :)
  *
  */

  require_once(dirname(__FILE__) . DIRECTORY_SEPARATOR . 'phpagi.php');

//  $fastagi = new AGI();
$phpagi_config_file = "/data/etc/asterisk/phpagi.conf";
$fastagi = new AGI($phpagi_config_file);

  $fastagi->verbose(print_r($fastagi, true));

  if(!isset($fastagi->config['fastagi']['basedir']))
    $fastagi->config['fastagi']['basedir'] = dirname(__FILE__);

  // perform some security checks

  $script = $fastagi->config['fastagi']['basedir'] . DIRECTORY_SEPARATOR . $fastagi->request['agi_network_script'];

  // in the same directory (or subdirectory)
  $mydir = dirname($fastagi->config['fastagi']['basedir']) . DIRECTORY_SEPARATOR;
  $dir = dirname($script) . DIRECTORY_SEPARATOR;
  if(substr($dir, 0, strlen($mydir)) != $mydir)
  {
    $fastagi->conlog("$script is not allowed to execute.");
    exit;
  }

  // make sure it exists
  if(!file_exists($script))
  {
    $fastagi->conlog("$script does not exist.");
    exit;
  }

$fastagi->conlog("$script exists and is executable.");

  // drop privileges
  if(isset($fastagi->config['fastagi']['setuid']) && $fastagi->config['fastagi']['setuid'])
  {
    $owner = fileowner($script);
    $group = filegroup($script);
$fastagi->conlog("current uid: ". posix_getuid());
$fastagi->conlog("current gid: ". posix_getgid());
$fastagi->conlog("script owner: $owner, group: $group");
    if(!posix_setgid($group) || !posix_setegid($group) || !posix_setuid($owner) || !posix_seteuid($owner))
    {
      $fastagi->conlog("failed to lower privileges.");
      exit;      
    }
  }

  // make sure script is still readable
  if(!is_readable($script))
  {
    $fastagi->conlog("$script is not readable.");
    exit;
  }

$fastagi->conlog("about to execute $script ...");

  require_once($script);
?>
