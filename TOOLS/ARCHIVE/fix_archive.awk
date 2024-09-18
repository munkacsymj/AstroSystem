{
 T_STR=substr($1, 3)
 F_STR=substr($2, 3)

 CMD= "add_archive -f " F_STR " -t " T_STR
 print CMD
 system(CMD)
 }
