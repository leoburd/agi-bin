<p>Hi,</p>
<p>We have just received a new audio announcement for <?php print "$person_name";?> 
   from telephone: <?php print htmlentities($caller_id['name'] . '<' . $caller_id['number'] . '>');?>.
<p>
- Your What's Up team
<?php print date('j M, Y \a\t g:i:s A', $date_created);?></p>
