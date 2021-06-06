
(defun print-limits ()
  (dolist (var '(
                 -
                 ;; 5 data control flow
                 lambda-list-keywords
                 nil
                 t

                 -
                 ;; 12 number
                 most-negative-fixnum                   
                 most-positive-fixnum                   
                 -
                 long-float-epsilon            
                 long-float-negative-epsilon   
                 most-negative-long-float               
                 most-positive-long-float               
                 least-negative-long-float              
                 least-positive-long-float              
                 least-negative-normalized-long-float   
                 least-positive-normalized-long-float   
                 -
                 double-float-epsilon          
                 double-float-negative-epsilon 
                 most-negative-double-float             
                 most-positive-double-float             
                 least-positive-double-float            
                 least-negative-double-float            
                 least-negative-normalized-double-float 
                 least-positive-normalized-double-float 
                 -
                 single-float-epsilon          
                 single-float-negative-epsilon 
                 most-negative-single-float             
                 most-positive-single-float             
                 least-negative-single-float            
                 least-positive-single-float            
                 least-negative-normalized-single-float 
                 least-positive-normalized-single-float 
                 -
                 short-float-epsilon           
                 short-float-negative-epsilon  
                 most-negative-short-float              
                 most-positive-short-float              
                 least-negative-short-float             
                 least-positive-short-float             
                 least-negative-normalized-short-float  
                 least-positive-normalized-short-float  
                 -
                 
                 boole-clr
                 boole-set
                 boole-1
                 boole-2
                 boole-c1
                 boole-c2
                 boole-and
                 boole-ior
                 boole-xor
                 boole-eqv
                 boole-nand
                 boole-nor
                 boole-andc1
                 boole-andc2
                 boole-orc1
                 boole-orc2
                 -
                 pi
                 -                
                 ;; 25 environment

                 internal-time-units-per-second
                 -
                 ;; limits
                 array-dimension-limit   
                 array-rank-limit        
                 array-total-size-limit  
                 call-arguments-limit    
                 char-code-limit         
                 lambda-parameters-limit 
                 multiple-values-limit   


                 ))
    (if (eq var '-)
        (progn
          (format t "|-~%")
          (format t "| Constant Variable                      |                    Value |~%")
          (format t "|-~%"))
        (format t "| =~:@(~A~)= | ~A |~%" var (symbol-value var)))))

(print-limits)

