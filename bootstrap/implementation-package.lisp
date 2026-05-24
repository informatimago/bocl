;;;;  -*- mode:lisp;coding:utf-8 -*-
;;;;**************************************************************************
;;;; FILE:               implementation-package.lisp
;;;; LANGUAGE:           Common-Lisp
;;;; SYSTEM:             Common-Lisp
;;;; USER-INTERFACE:     NONE
;;;; DESCRIPTION
;;;;     
;;;;     This file defines the Bootstrap Common Lisp implementation packages.
;;;;     
;;;; AUTHORS
;;;;     <PJB> Pascal Bourguignon <pjb@informatimago.com>
;;;; MODIFICATIONS
;;;;     2008-05-27 <PJB> Created.
;;;; BUGS
;;;; LEGAL
;;;;     GPL
;;;;     
;;;;     Copyright Pascal Bourguignon 2008 - 2008
;;;;     
;;;;     This program is free software; you can redistribute it and/or
;;;;     modify it under the terms of the GNU General Public License
;;;;     as published by the Free Software Foundation; either version
;;;;     2 of the License, or (at your option) any later version.
;;;;     
;;;;     This program is distributed in the hope that it will be
;;;;     useful, but WITHOUT ANY WARRANTY; without even the implied
;;;;     warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
;;;;     PURPOSE.  See the GNU General Public License for more details.
;;;;     
;;;;     You should have received a copy of the GNU General Public
;;;;     License along with this program; if not, write to the Free
;;;;     Software Foundation, Inc., 59 Temple Place, Suite 330,
;;;;     Boston, MA 02111-1307 USA
;;;; *************************************************************************


(defpackage "COM.INFORMATIMAGO.BOOTSTRAP.IMPLEMENTATION"
  (:use "COMMON-LISP"
        "COM.INFORMATIMAGO.BOOTSTRAP.BACKEND.STORAGE")
  (:export "NULL" "NIL"
           "CONS" "CAR" "CDR" "ATOM"
           )
  (:documentation "

A 'bootstrap' Common Lisp subset.

Copyright Pascal Bourguignon 2008 - 2008

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version
2 of the License, or (at your option) any later version.
"))


;;;; THE END ;;;;
