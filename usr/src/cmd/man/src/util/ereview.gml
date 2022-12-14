<!-- SCCS keyword
#pragma ident	"@(#)ereview.gml	1.2	97/04/24 SMI"
-->
<!--
ArborText: 19910809

$Header: ereview.gml,v 1.3 92/11/13 15:21:36 twb Exp $

The following declarations may be referred to using a public entity as
follows:

<!ENTITY % ereview PUBLIC "-//USA-DOD//DTD SUP MIL-M-28001B EREVIEW//EN">

-->

<!-- The %mrinfo entity is required for support of the electronic review
declaration set.  Note that this entity matches an identical set of elements
in the base document being reviewed, and may therefore require tailoring
accordingly.  For documents conforming to the Template Doctype for Technical
Documents contained in Appendix A of this specification, the %mrinfo entity is
declared as follows:

<!ENTITY   % mrinfo     "(pubno+, (revnum|(chgnum, chgdate)|pubdate))" >

-->

<!-- The %mrtext entity indicates what elements from the base DTD can occur in
the "textual" (i.e., mrpara and mritem) elements in a modreq.  For documents
conforming to the Template Doctype for Technical Documents contained in
Appendix A of this specification, the %mrtext entity is declared as follows:

<!ENTITY % mrtext       "#PCDATA | symbol" >

-->

<!-- The %mrelems entity indicates what elements from the base DTD can occur
along with the mrpara and mrlist elements (e.g., within mrreason, mrinstr,
mrgenmod, and mrrespns) in a modreq.  For documents conforming to the Template
Doctype for Technical Documents contained in Appendix A of this specification,
the %mrelems entity is declared as follows:

<!ENTITY %  mrelems     "| graphic" >

-->



<!-- Generic default definitions of %mrinfo, %mrtext, and %mrelems are given
below.  These are to be replaced by a definition appropriate to the document
being reviewed: -->

<!ENTITY % mrinfo       "ANY" >

<!ENTITY % mrtext       "#PCDATA" >

<!ENTITY % mrelems      " " >

<!-- Beginning of modification request declaration set -->
            
<!ELEMENT   modreq      - -   (mrinfo?, mrmod, mrrespns?) >
<!ATTLIST   modreq      id                ID                      #REQUIRED
                        xref              NMTOKEN                 #IMPLIED
                        refpos      (prexref|postxref|xref)       "xref"
                        by                CDATA                   #REQUIRED
                        date              CDATA                   #REQUIRED
                        organiz           NMTOKEN                 #IMPLIED
                        orgcat            NMTOKEN                 #IMPLIED
                        cmntrcat          NMTOKEN                 #IMPLIED
                        priority          (1|2|3|4|5)             #IMPLIED
                        category          NMTOKEN                 #IMPLIED
                        topic             CDATA                   #IMPLIED
                        locmodel          CDATA                   #IMPLIED  >
                        
<!ELEMENT   mrinfo      - -   %mrinfo; >
<!ELEMENT   mrmod - -   (mrreason?, (mrgenmod|(mrinstr?, mrchgtxt))) >
<!ELEMENT   (mrreason|mrinstr|mrgenmod) - o     (mrpara|mrlist %mrelems;)+ >
<!ELEMENT   mrchgtxt    - -   ANY >
<!ATTLIST   mrchgtxt    chgloc            NUMBER                  #IMPLIED
                        chglen            NUMBER                  #IMPLIED
                        action      (insert|delete|replace)       "replace" >
<!ELEMENT   mrrespns    - -   (mrpara|mrlist %mrelems;)* >
<!ATTLIST   mrrespns    disposn           NMTOKEN                 #IMPLIED
                        status            NMTOKEN                 #IMPLIED  > 
<!ELEMENT   (mrpara|mritem)         - -   (%mrtext;) >
<!ELEMENT   mrlist            - -   (mritem+) >

<!-- End of modification request declaration set -->
