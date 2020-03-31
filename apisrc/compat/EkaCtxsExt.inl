/*
 * EkaCtxsExt.inl
 */

/*
 *
 */
typedef struct {
    #define SqfSesCtx_FIELD_ITER( _x )                                      \
                _x( SesCtx,   common )                                      \
                _x( uint32_t, badge  )
    SqfSesCtx_FIELD_ITER( EKA__FIELD_DEF )
} SqfSesCtx;

/*
 *
 */
typedef struct {
    #define MeiSesCtx_FIELD_ITER( _x )                                      \
                _x( SesCtx,   common )                                      \
                _x( uint32_t, mpid   )
    MeiSesCtx_FIELD_ITER( EKA__FIELD_DEF )
} MeiSesCtx;
