(defpackage "COM.INFORMATIMAGO.BOCL.LOOP-FACILITY"
  (:use "COMMON-LISP")
  (:use "COM.INFORMATIMAGO.COMMON-LISP.LISP-SEXP.SOURCE-FORM"
        "COM.INFORMATIMAGO.RDP")
  (:export "PARSE-EXTENDED-LOOP"
           "GENERATE-EXTENDED-LOOP"))
(in-package "COM.INFORMATIMAGO.BOCL.LOOP-FACILITY")



(defgrammar extended-loop
  :terminals (symbol)
  :start loop
  :rules (
          (--> var                           symbol)
          (--> simple-var                    symbol)
          (--> form                          (or symbol list))
          (--> compound-form                 list)
          (--> vector                        form)
          
          (--> loop                          (opt name-clause) (rep variable-clause) (rep main-clause))
          (--> name-clause                   :named name)
          (--> name                          symbol)
          (--> variable-clause               (alt with-clause initial-final for-as-clause))
          (--> with-clause                   :with var (opt type-spec) (opt (seq := form)) (rep :and var (opt type-spec) (opt := form)))
          (--> main-clause                   (alt unconditional accumulation conditional termination-test initial-final))
          (--> initial-final                 (alt (seq :initially compound-form+) (seq :finally   compound-form+)))
          (--> compound-form+                compound-form (rep compound-form))
          (--> unconditional                 (alt (seq (alt :do :doing) compound-form+)) (seq :return (alt form :it)))
          (--> accumulation                  (alt list-accumulation numeric-accumulation))
          (--> list-accumulation             (alt :collect :collecting :append :appending :nconc :nconcing) (alt form :it) (opt (seq :into simple-var)))
          (--> numeric-accumulation          (alt :count :counting :sum :summing :maximize :maximizing :minimize :minimizing) (alt form :it) (opt (seq :into simple-var)) (alt type-spec))

          (--> conditional                   (alt :if :when :unless) form selectable-clause (rep (seq :and selectable-clause))
               (opt (seq :else selectable-clause (rep (seq :and selectable-clause)))) 
               (opt :end))
          
          (--> selectable-clause             (alt unconditional accumulation conditional))
          (--> termination-test              (alt (seq :while form) (seq :until form) (seq :repeat form) (seq :always form) (seq :never form) (seq :thereis form)))
          (--> for-as-clause                 (alt :for :as) for-as-subclause (rep :and for-as-subclause))
          (--> for-as-subclause              (alt for-as-arithmetic for-as-in-list for-as-on-list for-as-equals-then for-as-across for-as-hash for-as-package))
          (--> for-as-arithmetic             var (opt type-spec) for-as-arithmetic-subclause)
          (--> for-as-arithmetic-subclause   (alt arithmetic-up  arithmetic-downto  arithmetic-downfrom))
          (--> arithmetic-up                 (alt (seq (alt :from :upfrom) form) (seq (alt :to :upto :below) form) (seq :by form)) (rep (alt (seq (alt :from :upfrom) form) (seq (alt :to :upto :below) form) (seq :by form))))
          
          (--> arithmetic-downto             (alt 
                                              (seq                  :by form                :from form (alt :downto :above) form)
                                              (seq                  :by form (alt :downto :above) form                :from form)
                                              (seq                :from form                  :by form (alt :downto :above) form)
                                              (seq                :from form (alt :downto :above) form                  :by form)
                                              (seq                :from form (alt :downto :above) form)
                                              (seq (alt :downto :above) form                  :by form                :from form) 
                                              (seq (alt :downto :above) form                :from form                  :by form)
                                              (seq (alt :downto :above) form                :from form)))

          
          (--> arithmetic-downfrom           (alt
                                              (seq                      :by form                :downfrom form (alt :to :downto :above) form)
                                              (seq                      :by form (alt :to :downto :above) form :downfrom form)
                                              (seq                      :by form :downfrom form)
                                              (seq                :downfrom form                      :by form (alt :to :downto :above) form)
                                              (seq                :downfrom form                      :by form)
                                              (seq                :downfrom form (alt :to :downto :above) form                      :by form)
                                              (seq                :downfrom form (alt :to :downto :above) form)
                                              (seq (alt :to :downto :above) form                      :by form :downfrom form)
                                              (seq (alt :to :downto :above) form                :downfrom form                      :by form)
                                              (seq (alt :to :downto :above) form :downfrom form)))

          
          (--> for-as-in-list                  var (opt type-spec) :in form (opt (seq :by step-fun)))
          (--> for-as-on-list                  var (opt type-spec) :on form (opt (seq :by step-fun)))
          (--> for-as-equals-then              var (opt type-spec) := form (opt (seq :then form)))
          (--> for-as-across                   var (opt type-spec) :across vector)
          (--> for-as-hash                     var (opt type-spec) :being (alt :each :the)
               (alt (seq (alt :hash-key :hash-keys) (alt :in :of) :hash-table (alt (opt :using (:hash-value other-var))))
                    (seq (alt :hash-value :hash-values) (alt :in :of) :hash-table (alt (opt :using (:hash-key other-var))))))
          (--> for-as-package                  var (opt type-spec) :being (alt :each :the)
               (alt :symbol :symbols :present-symbol :present-symbols :external-symbol :external-symbols)
               (opt (alt :in :of) package))
          (--> type-spec (alt simple-type-spec destructured-type-spec))
          (--> simple-type-spec (alt fixnum float t nil))
          (--> destructured-type-spec :of-type d-type-spec )
          (--> d-type-spec   (alt type-specifier  ( d-type-spec . d-type-spec ) ))
          (--> var d-var-spec)
          (--> other-var d-var-spec )
          (--> d-var-spec (alt simple-var  nil  ( d-var-spec . d-var-spec ) )))
  :scanner t
  :skip-spaces t
  :eof-symbol *eof-symbol*
  :target-language :lisp
  :trace nil)




