BEGIN { found = 0; }

{ if ( ( found == 0 ) && ($3 ~ /dpid_password/) )  
   {
        printf ("\"dpid2\"       1.3.6.1.4.1.2.3.1.2.2.1.1.2       \"dpid_password\"\n")
        found = 1;
   }
  else
   {
     print $0;
   }
}    

END { if ( found == 0 )
        printf ("\"dpid2\"       1.3.6.1.4.1.2.3.1.2.2.1.1.2       \"dpid_password\"\n") 
    }
