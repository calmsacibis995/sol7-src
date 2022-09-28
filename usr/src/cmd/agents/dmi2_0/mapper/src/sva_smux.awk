BEGIN { found = 0; }

{ if ( ( found == 0 ) && ($3 ~ /dpid_password/) )  
   {
        printf ("smux        1.3.6.1.4.1.2.3.1.2.2.1.1.2         dpid_password     #dpid\n")
        found = 1;
   }
  else
   {
     print $0;
   }
}    

END { if ( found == 0 )
        printf ("smux        1.3.6.1.4.1.2.3.1.2.2.1.1.2         dpid_password     #dpid\n") 
    }
