####  -*- mode:shell-script-mode;coding:utf-8 -*-
####**************************************************************************
#### FILE:               bash/native.sh
#### LANGUAGE:           Common-Lisp
#### SYSTEM:             Common-Lisp
#### USER-INTERFACE:     NONE
#### DESCRIPTION
####     
####    Target Bash.
####    Implementation of the backend native data types.
####
#### AUTHORS
####     <PJB> Pascal Bourguignon <pjb@informatimago.com>
#### MODIFICATIONS
####     2009-01-24 <PJB> Created.
#### BUGS
#### LEGAL
####     GPL
####     
####     Copyright Pascal Bourguignon 2009 - 2009
####     
####     This program is free software; you can redistribute it and/or
####     modify it under the terms of the GNU General Public License
####     as published by the Free Software Foundation; either version
####     2 of the License, or (at your option) any later version.
####     
####     This program is distributed in the hope that it will be
####     useful, but WITHOUT ANY WARRANTY; without even the implied
####     warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
####     PURPOSE.  See the GNU General Public License for more details.
####     
####     You should have received a copy of the GNU General Public
####     License along with this program; if not, write to the Free
####     Software Foundation, Inc., 59 Temple Place, Suite 330,
####     Boston, MA 02111-1307 USA
#### *************************************************************************

# (defpackage "COM.INFORMATIMAGO.BOOTSTRAP.BACKEND.J-MACRO.NATIVE"
#   (:use "COM.INFORMATIMAGO.J-LANGUAGE"
#         "COM.INFORMATIMAGO.BOOTSTRAP.BACKEND.NATIVE")
#   (:export
#    "FIXNUM+" "FIXNUM-" "FIXNUM*" "FIXNUM-TRUNCATE" "FIXNUM<" "FIXNUM<="
#    "FIXNUM>" "FIXNUM>=" "FIXNUM=" "FIXNUM/=" "CHARACTER<" "CHARACTER<="
#    "CHARACTER>" "CHARACTER>=" "CHARACTER=" "CHARACTER/="
#    "CHARACTER-EQUALP" "CHARACTER-LESSP" "CHARACTER-GREATERP"
#    "STRING-LENGTH" "STRING-REF" "STRING-SET" "STRING-SUBSEQ" "STRING<"
#    "STRING<=" "STRING>" "STRING>=" "STRING=" "STRING/=" "STRING-EQUALP"
#    "STRING-LESSP" "STRING-GREATERP" "STRING-COPY" "FIXNUM-TO-STRING"
#    "FIXNUM-TO-CHARACTER" "STRING-TO-FIXNUM" "STRING-TO-CHARACTER"
#    "CHARACTER-TO-FIXNUM" "CHARACTER-TO-STRING" 
#    )
#   (:documentation "
# Target (Objecteering) J macros.
# Implementation of the backend native data types.
# 
# Note: this is a common lisp implementation of the storage, as it
#       will be implemented in J macro language.
# 
# Copyright Pascal Bourguignon 2008 - 2008
#  
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version
# 2 of the License, or (at your option) any later version.
# "))
# 
# (in-package "COM.INFORMATIMAGO.BOOTSTRAP.BACKEND.J-MACRO.NATIVE")

value0=x
value1=x
value2=x
value3=x
value_count=1
error=0
error_message=x


################## FIXNUM ####################

fixnum_plus(){
    value0=$(( $1 + $2 ))
    value_count=1
}

fixnum_minus(){
    value0=$(( $1 - $2 ))
    value_count=1
}

fixnum_times(){
    value0=$(( ( $1 * $2 ) / 16 ))
    value_count=1
}

fixnum_truncate(){
    value0=$(( 16 * ( $1 / $2 ) ))
    value1=$(( 16 * ( ( $1 / 16 ) % ( $2 / 16 ) )  ))
    value_count=2
}


fixnum_lt(){
    value_count=1
    if (( $1 < $2 )) ; then
        value0=1
    else
        value0=0
    fi
}

fixnum_le(){
    value_count=1
    if (( $1 <= $2 )) ; then
        value0=1
    else
        value0=0
    fi
}

fixnum_gt(){
    value_count=1
    if (( $1 > $2 )) ; then
        value0=1
    else
        value0=0
    fi
}

fixnum_ge(){
    value_count=1
    if (( $1 >= $2 )) ; then
        value0=1
    else
        value0=0
    fi
}

fixnum_eq(){
    value_count=1
    if (( $1 == $2 )) ; then
        value0=1
    else
        value0=0
    fi
}

fixnum_ne(){
    value_count=1
    if (( $1 != $2 )) ; then
        value0=1
    else
        value0=0
    fi
}




################## CHARACTER ####################


character_lt(){
    value_count=1
    if [[ "$1" < "$2" ]] ; then
        value0=1
    else
        value0=0
    fi
}

character_le(){
    value_count=1
    if [[ ! ( "$1" > "$2" ) ]] ; then
        value0=1
    else
        value0=0
    fi
}


character_gt(){
    value_count=1
    if [[ "$1" > "$2" ]] ; then
        value0=1
    else
        value0=0
    fi
}

character_ge(){
    value_count=1
    if [[ ! ( "$1" < "$2" ) ]] ; then
        value0=1
    else
        value0=0
    fi
}


character_eq(){
    value_count=1
    if [[ "$1" == "$2" ]] ; then
        value0=1
    else
        value0=0
    fi
}

character_ne(){
    value_count=1
    if [[ "$1" != "$2" ]] ; then
        value0=1
    else
        value0=0
    fi
}



character_to_lower(){
    value_count=1
    case "$1" in
    A) value0=a ;;
    B) value0=b ;;
    C) value0=c ;;
    D) value0=d ;;
    E) value0=e ;;
    F) value0=f ;;
    G) value0=g ;;
    H) value0=h ;;
    I) value0=i ;;
    J) value0=j ;;
    K) value0=k ;;
    L) value0=l ;;
    M) value0=m ;;
    N) value0=n ;;
    O) value0=o ;;
    P) value0=p ;;
    Q) value0=q ;;
    R) value0=r ;;
    S) value0=s ;;
    T) value0=t ;;
    U) value0=u ;;
    V) value0=v ;;
    W) value0=w ;;
    X) value0=x ;;
    Y) value0=y ;;
    Z) value0=z ;;
    *) value0="$1" ;;
    esac
}

character_to_upper(){
    value_count=1
    case "$1" in
    a) value0=A ;;
    b) value0=B ;;
    c) value0=C ;;
    d) value0=D ;;
    e) value0=E ;;
    f) value0=F ;;
    g) value0=G ;;
    h) value0=H ;;
    i) value0=I ;;
    j) value0=J ;;
    k) value0=K ;;
    l) value0=L ;;
    m) value0=M ;;
    n) value0=N ;;
    o) value0=O ;;
    p) value0=P ;;
    q) value0=Q ;;
    r) value0=R ;;
    s) value0=S ;;
    t) value0=T ;;
    u) value0=U ;;
    v) value0=V ;;
    w) value0=W ;;
    x) value0=X ;;
    y) value0=Y ;;
    z) value0=Z ;;
    *) value0="$1" ;;
    esac
}

character_equalp(){
    character_to_lower $1
    local l=$value0
    character_to_lower $2
    local r=$value0
    character_eq "$l" "$r"
}

character_lessp(){
    character_to_lower $1
    local l=$value0
    character_to_lower $2
    local r=$value0
    character_lt "$l" "$r"
}

character_greaterp(){
    character_to_lower $1
    local l=$value0
    character_to_lower $2
    local r=$value0
    character_gt "$l" "$r"
}


# Lisp strings are mutable, but native bash strings are not mutable.
# In shell, we can select a character in a string, ${string:$i:$l} but
# we cannot change it.  On the other hand, we can change a slot in an
# array. string[$i]=$c But we cannot put arrays inside arrays.
# Therefore strings will be stored as arrays in "indexed" variables,
# string_$i


# All these string_ function take pointers in the form "string_$i" as
# string arguments.


# We cannot use $() to call procedures, only pure functions. Side
# effects are not applied on the calling process with $()...
# Therefore procedures must give their results thru a global variable.

next_string=0

make_string(){ # length initchar --> string_$s
    local len="$1"
    local s=$next_string
    next_string=$(( $next_string + 1 ))
    eval "for (( i=0 ; i<$len ; i++ )) ; do string_$s[\$i]='$2' ; done"
    value0=string_$s
    value_count=1
}

make_string_from_native(){ # "string" --> string_$s
    local inp="$1"
    local len=${#inp}
    local s=$next_string
    next_string=$(( $next_string + 1 ))
    eval "for (( i=0 ; i<$len ; i++ )) ; do string_$s[\$i]=\"\${inp:\${i}:1}\" ; done"
    value0=string_$s
    value_count=1
}

string_to_native(){ # string_$s --> "string"
    value0=""
    local len=0
    eval "len=\${#$1[@]}"
    for (( i=0 ; i<$len ; i++ )) ; do
        eval "value0=\"\${result}\${$1[\$i]}\""
    done
    value_count=1
}

string_length(){ # string_$s
    eval "value0=\${#$1[@]}"
    value_count=1
}

string_ref(){ # string_$s $index
    string_length $1
    local len=$value0
    value_count=1
    if [[ ! ( $2 < 0 ) &&  $2 < $len ]] ; then
        eval "value0=\${$1[$2]}"
    else
        value0=""
        error=1
        error_message="string ref index out of range"
    fi
}

string_set(){ # string_$s $index $character
    string_length $1
    local len=$value0
    value_count=1
    if [[ ! ( 0 > $2 ) && $2 < $len ]] ; then
        eval "$1[$2]=\'$3\'"
        value0="$3"
    else
        error=1
        error_message="string set index out of range"
    fi
}

string_subseq(){ # string$s $start $end
    local str=$1
    local start=$2
    local end=$3
    string_length $str
    local strlen=$value0
    local res
    if [ 0 -le $start -a $start -le $end -a $end -lt $strlen ] ; then
        make_string $(( $end - $start ))
        res=$value0
        for (( i=0 ; i<( $end - $start ) ; i++ )) ; do
            eval "result[\$i]=\"\${$res[\$((\$i + $start))]}\""
        done
        value0=$res
        error=0
    else
        value0=$str
        error=1
    fi
}


string_copy(){ # string$s
    string_length $1
    local len=$value0
    make_string $len
    local res=$value0
    for (( i=0 ; i<$len ; i++ )) ; do
        eval "\$res[\$i]=\"\${$1[\$i]}\""
    done
    value_count=1
    value0=$res
}
    

# a<=b <=> not b<a
# a>=b <=> not a<b
# a>b  <=> b<a

string_le(){ # string$a string$b
    string_lt $2 $1
    value0=$(( ! $value0 ))
}

string_ge(){ # string$a string$b
    string_lt $1 $2
    value0=$(( ! $value0 ))
}

string_gt(){ # string$a string$b
    string_lt $2 $1
}

string_lt(){ # string$a string$b
    local i=0
    string_length $1
    local la=$value0
    string_length $2
    local lb=$value0
    if [[ $i < $la && $i < $lb ]] ; then
        character_eq "$(eval "\"\${$1[\$i]}\"")"  "$(eval "\"\${$2[\$i]}\"")"
        while [[ $i < $la && $i < $lb && $value0 ]]; do
            i=$(( $i + 1 ))
            if [[ $i < $la && $i < $lb ]] ; then
                character_eq "$(eval "\"\${$1[\$i]}\"")"  "$(eval "\"\${$2[\$i]}\"")"
            fi
        done
    fi
    value_count=1
    if (( $i == $la )) ; then
        if (( $i == $lb )) ; then
            # equal
            value0=0
        else
            # a<b
            value0=1
        fi
    else
        if (( $i == $lb )) ; then
            # a>b
            value0=0
        else
            character_lt "$(eval "\"\${$1[\$i]}\"")" "$(eval "\"\${$2[\$i]}\"")"
        fi
    fi
}


string_eq(){ # string$a string$b
    local i=0
    string_length $1
    local la=$value0
    string_length $2
    local lb=$value0
    value_count=1
    if (( $la == $lb )) ; then
        if [[ $i < $la && $i < $lb ]] ; then
            character_eq "$(eval "\"\${$1[\$i]}\"")"  "$(eval "\"\${$2[\$i]}\"")"
            while [[ $i < $la && $i < $lb && $value0 ]]; do
                i=$(( $i + 1 ))
                if [[ $i < $la && $i < $lb ]] ; then
                    character_eq "$(eval "\"\${$1[\$i]}\"")"  "$(eval "\"\${$2[\$i]}\"")"
                fi
            done
        fi
        if (( $i == $la )) ; then
            if (( $i == $lb )) ; then
            # equal
                value0=1
            else
            # a<b
                value0=0
            fi
        else
            value0=0
        fi 
    else
        value0=0
    fi
}


string_ne(){ # string$a string$b 
    string_eq "$1" "$2"
    value0=$(( ! $value0 ))
}



string_equalp(){  # string$a string$b
    local i=0
    string_length $1
    local la=$value0
    string_length $2
    local lb=$value0
    value_count=1
    if (( $la == $lb )) ; then
        if [[ $i < $la && $i < $lb ]] ; then
            character_equalp "$(eval "\"\${$1[\$i]}\"")" "$(eval "\"\${$2[\$i]}\"")"
            while [[ $i < $la && $i < $lb && $value0 ]]; do
                i=$(( $i + 1 ))
                if [[ $i < $la && $i < $lb ]] ; then
                    character_equalp "$(eval "\"\${$1[\$i]}\"")" "$(eval "\"\${$2[\$i]}\"")"
                fi
            done
        fi
        if (( $i == $la )) ; then
            if (( $i == $lb )) ; then
            # equal
                value0=1
            else
            # a<b
                value0=0
            fi
        else
            value0=0
        fi 
    else
        value0=0
    fi
}


string_lessp(){  # string$a string$b
    local i=0
    string_length $1
    local la=$value0
    string_length $2
    local lb=$value0
    value_count=1
    if [[ $i < $la && $i < $lb ]] ; then
        character_equalp "$(eval "\"\${$1[\$i]}\"")" "$(eval "\"\${$2[\$i]}\"")"
        while [[ $i < $la && $i < $lb && $value0 ]]; do
            i=$(( $i + 1 ))
            if [[ $i < $la && $i < $lb ]] ; then
                character_equalp "$(eval "\"\${$1[\$i]}\"")" "$(eval "\"\${$2[\$i]}\"")"
            fi
        done
    fi
    if (( $i == $la )) ; then
        if (( $i == $lb )) ; then
            # equal
            value0=0
        else
            # a<b
            value0=1
        fi
    else
        if (( $i == $lb )) ; then
            # a>b
            value0=0
        else
            character_lessp "$(eval "\"\${$1[\$i]}\"")" "$(eval "\"\${$2[\$i]}\"")"
        fi
    fi
}

string_greaterp(){ # string$a string$b
    string_lessp $2 $1
}



ascii=( \
    '#' '#' '#' '#' '#' '#' '#' '#' '#' '#' '#' '#' '#' '#' '#' '#' \
    '#' '#' '#' '#' '#' '#' '#' '#' '#' '#' '#' '#' '#' '#' '#' '#' \
    ' ' '!' '"' '#' '$' '%' '&' "'" '(' ')' '*' '+' ',' '-' '.' '/' \
    '0' '1' '2' '3' '4' '5' '6' '7' '8' '9' ':' ';' '<' '=' '>' '?' \
    '@' 'A' 'B' 'C' 'D' 'E' 'F' 'G' 'H' 'I' 'J' 'K' 'L' 'M' 'N' 'O' \
    'P' 'Q' 'R' 'S' 'T' 'U' 'V' 'W' 'X' 'Y' 'Z' '[' '\\' ']' '^' '_' \
    '\`' 'a' 'b' 'c' 'd' 'e' 'f' 'g' 'h' 'i' 'j' 'k' 'l' 'm' 'n' 'o' \
    'p' 'q' 'r' 's' 't' 'u' 'v' 'w' 'x' 'y' 'z' '{' '|' '}' '~' '#')

fixnum_to_character(){ # fixnum -> character
    if (( $1 == 10 )) ; then
        value_count=1
        value0='
'
    elif (( $1 < 32 || 127 < $1 )) ; then
        error=1
        error_message="Character code $1 out of bounds"
        value_count=0
    else
        value_count=1
        value0="${ascii[$1]}"
    fi
}


character_to_fixnum(){ # character -> fixnum
    local i=32
    while [ $i -le 127 -a "x${ascii[$i]}" != "x$1" ]
    do
        i=$(( $i + 1 ))
    done
    if [ "x$1" = 'x
' ]
    then
        value_count=1
        value0=10
    elif [ $i -le 127 ]
    then
        value_count=1
        value0=$i
    else
        value_count=0
        error=1
        error_message="Expected a character"
    fi
}


fixnum_to_string(){
    make_string_from_native $1
}


string_to_fixnum(){ # string$s -> fixnum
    string_to_native
}


#### THE END ####

