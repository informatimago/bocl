#ifndef limits_h
#define limits_h

#define ARRAY_TOTAL_SIZE_LIMIT    (32768)
#define ARRAY_DIMENSION_LIMIT     (32768)
#define ARRAY_RANK_LIMIT          (   64)
#define CALL_ARGUMENTS_LIMIT      (   60)
#define CHAR_CODE_LIMIT           (  256)
#define LAMBDA_PARAMETERS_LIMIT   (   50)
#define MULTIPLE_VALUES_LIMIT     (   20)

#define STRING_STREAM_BUFFER_SIZE ARRAY_TOTAL_SIZE_LIMIT


#define INTEGER_INTERN_MIN -32
#define INTEGER_INTERN_MAX CHAR_CODE_LIMIT

#endif
