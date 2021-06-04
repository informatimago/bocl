#include "character.h"

/*
(loop for i from 32 to 255 do (insert(format "%c"i)))
ABCDEFGHIJKLMNOPQRSTUVWXYZ
abcdefghijklmnopqrstuvwxyz
ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏÐÑÒÓÔÕÖ×ØÙÚÛÜÝÞ
àáâãäåæçèéêëìíîïðñòóôõö÷øùúûüýþ

(- ?a ?A)32
(- ?à ?À)32
?a 97
?z 122
?à 224
?÷ 247
?þ 254
*/

bool printablep(octet ch){
    return !((ch<32) || ((127<=ch)&&(ch<0xA0)));}

bool constituentp(octet ch){
    if(160<=ch){
        return true;}
    switch(ch){
      case ' ':
      case '"':
      case '#':
      case '\'':
      case '(':
      case ')':
      case ',':
      case '\\':
      case '`':
      case '|':
          return false;}
    if((33<=ch)&&(ch<127)){
        return true;}
    return false;}

bool whitespacep(octet character){
    switch(character){
      case 9: /* TAB */
      case 10: /* LF */
      case 11: /* VT */
      case 12: /* FF */
      case 13: /* CR */
      case 32: /* SPC */
      case 0x85: /* NEL */
          return true;
      default:
          return false;}}

/**** THE END ****/
