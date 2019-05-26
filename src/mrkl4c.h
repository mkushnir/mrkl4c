#ifndef MRKL4C_H_DEFINED
#define MRKL4C_H_DEFINED

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/stat.h>

#include <mrkcommon/array.h>
#include <mrkcommon/bytes.h>
#include <mrkcommon/bytestream.h>
#include <mrkcommon/util.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MRKL4C_LOGGER_INVALID (-1)
typedef int mrkl4c_logger_t;

struct _mrkl4c_ctx;


typedef struct _mrkl4c_minfo {
    int id;
    /*
     * LOG_*
     */
    int flevel;
    int elevel;
    mnbytes_t *name;
    double throttle_threshold;
    int nthrottled;
} mrkl4c_minfo_t;


#define MRKL4C_FWRITER_DEFAULT_OPEN_FLAGS (O_WRONLY | O_APPEND | O_CREAT)
#define MRKL4C_FWRITER_DEFAULT_OPEN_MODE 0644
typedef struct _mrkl4c_writer {
    void (*write)(struct _mrkl4c_ctx *);
    union {
        struct {
            mnbytes_t *path;
            mnbytes_t *shadow_path;
            size_t cursz;
            size_t maxsz;
            double maxtm;
            double starttm;
            double curtm;
            size_t maxfiles;
            int fd;
            struct stat sb;
            unsigned flags;
        } file;
    } data;
} mrkl4c_writer_t;


typedef struct _mrkl4c_cache {
    pid_t pid;
} mrkl4c_cache_t;


#define MRKL4C_MAX_MINFOS 1024
typedef struct _mrkl4c_ctx {
    ssize_t nref;
    mnbytestream_t bs;
    ssize_t bsbufsz;
    /* strongref */
    mrkl4c_writer_t writer;
    mrkl4c_cache_t cache;
    mnarray_t minfos;
    unsigned ty;
} mrkl4c_ctx_t;

double mrkl4c_now_posix(void);

#define MRKL4C_OPEN_STDOUT  0x0001
#define MRKL4C_OPEN_STDERR  0x0002
#define MRKL4C_OPEN_FILE    0x0003
#define MRKL4C_OPEN_TY      0x00ff
#define MRKL4C_OPEN_FLOCK   0x0100



mrkl4c_logger_t mrkl4c_open(unsigned, ...);

#define MRKL4C_OPEN_FROM_FILE(path, maxsz, maxtm, maxbkp, flags)       \
mrkl4c_open(                                                           \
    MRKL4C_OPEN_FILE,                                                  \
    MNTYPECHK(char *, (path)),                                         \
    MNTYPECHK(size_t, (maxsz)),                                        \
    MNTYPECHK(double, (maxtm)),                                        \
    MNTYPECHK(size_t, (maxbkp)),                                       \
    MNTYPECHK(int, (flags)))                                           \


int mrkl4c_set_bufsz(mrkl4c_logger_t, ssize_t);
mrkl4c_logger_t mrkl4c_incref(mrkl4c_logger_t);
mrkl4c_ctx_t *mrkl4c_get_ctx(mrkl4c_logger_t);
int mrkl4c_traverse_minfos(mrkl4c_logger_t, array_traverser_t, void *);
bool mrkl4c_ctx_allowed(mrkl4c_ctx_t *, int, int);
int mrkl4c_close(mrkl4c_logger_t);
void mrkl4c_register_msg(mrkl4c_logger_t, int, int, const char *);
int mrkl4c_set_level(mrkl4c_logger_t, int, mnbytes_t *);
int mrkl4c_set_throttling(mrkl4c_logger_t, double, mnbytes_t *);
void mrkl4c_init(void);
void mrkl4c_fini(void);

UNUSED static const char *level_names[] = {
    "EMERG",
    "ALERT",
    "CRIT",
    "ERROR",
    "WARNING",
    "NOTICE",
    "INFO",
    "DEBUG",
};


/*
 * may be flevel
 */
#define MRKL4C_WRITE_MAYBE_PRINTFLIKE_FLEVEL(ld, mod, msg, ...)                        \
    do {                                                                               \
        mrkl4c_ctx_t *_mrkl4c_ctx;                                                     \
        mrkl4c_minfo_t *_mrkl4c_minfo;                                                 \
        _mrkl4c_ctx = mrkl4c_get_ctx(ld);                                              \
        assert(_mrkl4c_ctx != NULL);                                                   \
        _mrkl4c_minfo = ARRAY_GET(                                                     \
            mrkl4c_minfo_t,                                                            \
            &_mrkl4c_ctx->minfos,                                                      \
            mod ## _ ## msg ## _ID);                                                   \
        if (mrkl4c_ctx_allowed(_mrkl4c_ctx,                                            \
                               _mrkl4c_minfo->flevel,                                  \
                               mod ## _ ## msg ## _ID)) {                              \
            ssize_t _mrkl4c_nwritten;                                                  \
            double _mrkl4c_curtm;                                                      \
            assert(_mrkl4c_ctx->writer.write != NULL);                                 \
            _mrkl4c_curtm = mrkl4c_now_posix();                                        \
            if (_mrkl4c_ctx->writer.data.file.curtm +                                  \
                    _mrkl4c_minfo->throttle_threshold <= _mrkl4c_curtm) {              \
                _mrkl4c_ctx->writer.data.file.curtm = _mrkl4c_curtm;                   \
                _mrkl4c_nwritten = bytestream_nprintf(&_mrkl4c_ctx->bs,                \
                                              _mrkl4c_ctx->bsbufsz,                    \
                                              "%.06lf [%d] %s %s[%d]: "                \
                                              mod ## _ ## msg ## _FMT,                 \
                                              _mrkl4c_ctx->                            \
                                                writer.data.file.curtm,                \
                                              _mrkl4c_ctx->cache.pid,                  \
                                              mod ## _NAME,                            \
                                              level_names[_mrkl4c_minfo->flevel],      \
                                              _mrkl4c_minfo->                          \
                                                nthrottled,                            \
                                              ##__VA_ARGS__);                          \
                if (_mrkl4c_nwritten < 0) {                                            \
                    bytestream_rewind(&_mrkl4c_ctx->bs);                               \
                } else {                                                               \
                    SADVANCEPOS(&_mrkl4c_ctx->bs, -1);                                 \
                    (void)bytestream_cat(&_mrkl4c_ctx->bs, 1, "\n");                   \
                    if (SEOD(&_mrkl4c_ctx->bs) >= _mrkl4c_ctx->bsbufsz) {              \
                        _mrkl4c_ctx->writer.write(_mrkl4c_ctx);                        \
                    }                                                                  \
                }                                                                      \
                _mrkl4c_minfo->nthrottled = 0;                                         \
            } else {                                                                   \
                ++_mrkl4c_minfo->nthrottled;                                           \
            }                                                                          \
        }                                                                              \
    } while (0)                                                                        \


/*
 * may be context flevel
 */
#define MRKL4C_WRITE_MAYBE_PRINTFLIKE_CONTEXT_FLEVEL(                                  \
        ld, context, mod, msg, ...)                                                    \
    do {                                                                               \
        mrkl4c_ctx_t *_mrkl4c_ctx;                                                     \
        mrkl4c_minfo_t *_mrkl4c_minfo;                                                 \
        _mrkl4c_ctx = mrkl4c_get_ctx(ld);                                              \
        assert(_mrkl4c_ctx != NULL);                                                   \
        _mrkl4c_minfo = ARRAY_GET(                                                     \
            mrkl4c_minfo_t,                                                            \
            &_mrkl4c_ctx->minfos,                                                      \
            mod ## _ ## msg ## _ID);                                                   \
        if (mrkl4c_ctx_allowed(_mrkl4c_ctx,                                            \
                               _mrkl4c_minfo->flevel,                                  \
                               mod ## _ ## msg ## _ID)) {                              \
            ssize_t _mrkl4c_nwritten;                                                  \
            double _mrkl4c_curtm;                                                      \
            assert(_mrkl4c_ctx->writer.write != NULL);                                 \
            _mrkl4c_curtm = mrkl4c_now_posix();                                        \
            _mrkl4c_ctx->writer.data.file.curtm = mrkl4c_now_posix();                  \
            if (_mrkl4c_ctx->writer.data.file.curtm +                                  \
                    _mrkl4c_minfo->throttle_threshold <= _mrkl4c_curtm) {              \
                _mrkl4c_ctx->writer.data.file.curtm = _mrkl4c_curtm;                   \
                _mrkl4c_nwritten = bytestream_nprintf(&_mrkl4c_ctx->bs,                \
                                              _mrkl4c_ctx->bsbufsz,                    \
                                              "%.06lf [%d] %s %s[%d]: "                \
                                              context                                  \
                                              mod ## _ ## msg ## _FMT,                 \
                                              _mrkl4c_ctx->                            \
                                                writer.data.file.curtm,                \
                                              _mrkl4c_ctx->cache.pid,                  \
                                              mod ## _NAME,                            \
                                              level_names[_mrkl4c_minfo->flevel],      \
                                              _mrkl4c_minfo->                          \
                                                nthrottled,                            \
                                              ##__VA_ARGS__);                          \
                if (_mrkl4c_nwritten < 0) {                                            \
                    bytestream_rewind(&_mrkl4c_ctx->bs);                               \
                } else {                                                               \
                    SADVANCEPOS(&_mrkl4c_ctx->bs, -1);                                 \
                    (void)bytestream_cat(&_mrkl4c_ctx->bs, 1, "\n");                   \
                    if (SEOD(&_mrkl4c_ctx->bs) >= _mrkl4c_ctx->bsbufsz) {              \
                        _mrkl4c_ctx->writer.write(_mrkl4c_ctx);                        \
                    }                                                                  \
                }                                                                      \
                _mrkl4c_minfo->nthrottled = 0;                                         \
            } else {                                                                   \
                ++_mrkl4c_minfo->nthrottled;                                           \
            }                                                                          \
        }                                                                              \
    } while (0)                                                                        \


/*
 * may be
 */
#define MRKL4C_WRITE_MAYBE_PRINTFLIKE(ld, level, mod, msg, ...)                \
    do {                                                                       \
        mrkl4c_ctx_t *_mrkl4c_ctx;                                             \
        _mrkl4c_ctx = mrkl4c_get_ctx(ld);                                      \
        assert(_mrkl4c_ctx != NULL);                                           \
        if (mrkl4c_ctx_allowed(_mrkl4c_ctx, level, mod ## _ ## msg ## _ID)) {  \
            ssize_t _mrkl4c_nwritten;                                          \
            double _mrkl4c_curtm;                                              \
            mrkl4c_minfo_t *_mrkl4c_minfo;                                     \
            assert(_mrkl4c_ctx->writer.write != NULL);                         \
            _mrkl4c_curtm = mrkl4c_now_posix();                                \
            _mrkl4c_minfo = ARRAY_GET(                                         \
                mrkl4c_minfo_t,                                                \
                &_mrkl4c_ctx->minfos,                                          \
                mod ## _ ## msg ## _ID);                                       \
            if (_mrkl4c_ctx->writer.data.file.curtm +                          \
                    _mrkl4c_minfo->throttle_threshold <= _mrkl4c_curtm) {      \
                _mrkl4c_ctx->writer.data.file.curtm = _mrkl4c_curtm;           \
                _mrkl4c_nwritten = bytestream_nprintf(&_mrkl4c_ctx->bs,        \
                                              _mrkl4c_ctx->bsbufsz,            \
                                              "%.06lf [%d] %s %s[%d]: "        \
                                              mod ## _ ## msg ## _FMT,         \
                                              _mrkl4c_ctx->                    \
                                                writer.data.file.curtm,        \
                                              _mrkl4c_ctx->cache.pid,          \
                                              mod ## _NAME,                    \
                                              level_names[level],              \
                                              _mrkl4c_minfo->                  \
                                                nthrottled,                    \
                                              ##__VA_ARGS__);                  \
                if (_mrkl4c_nwritten < 0) {                                    \
                    bytestream_rewind(&_mrkl4c_ctx->bs);                       \
                } else {                                                       \
                    SADVANCEPOS(&_mrkl4c_ctx->bs, -1);                         \
                    (void)bytestream_cat(&_mrkl4c_ctx->bs, 1, "\n");           \
                    if (SEOD(&_mrkl4c_ctx->bs) >= _mrkl4c_ctx->bsbufsz) {      \
                        _mrkl4c_ctx->writer.write(_mrkl4c_ctx);                \
                    }                                                          \
                }                                                              \
                _mrkl4c_minfo->nthrottled = 0;                                 \
            } else {                                                           \
                ++_mrkl4c_minfo->nthrottled;                                   \
            }                                                                  \
        }                                                                      \
    } while (0)                                                                \


/*
 * may be context
 */
#define MRKL4C_WRITE_MAYBE_PRINTFLIKE_CONTEXT(                                 \
        ld, level, context, mod, msg, ...)                                     \
    do {                                                                       \
        mrkl4c_ctx_t *_mrkl4c_ctx;                                             \
        _mrkl4c_ctx = mrkl4c_get_ctx(ld);                                      \
        assert(_mrkl4c_ctx != NULL);                                           \
        if (mrkl4c_ctx_allowed(_mrkl4c_ctx, level, mod ## _ ## msg ## _ID)) {  \
            ssize_t _mrkl4c_nwritten;                                          \
            double _mrkl4c_curtm;                                              \
            mrkl4c_minfo_t *_mrkl4c_minfo;                                     \
            assert(_mrkl4c_ctx->writer.write != NULL);                         \
            _mrkl4c_curtm = mrkl4c_now_posix();                                \
            _mrkl4c_minfo = ARRAY_GET(                                         \
                mrkl4c_minfo_t,                                                \
                &_mrkl4c_ctx->minfos,                                          \
                mod ## _ ## msg ## _ID);                                       \
            _mrkl4c_ctx->writer.data.file.curtm = mrkl4c_now_posix();          \
            if (_mrkl4c_ctx->writer.data.file.curtm +                          \
                    _mrkl4c_minfo->throttle_threshold <= _mrkl4c_curtm) {      \
                _mrkl4c_ctx->writer.data.file.curtm = _mrkl4c_curtm;           \
                _mrkl4c_nwritten = bytestream_nprintf(&_mrkl4c_ctx->bs,        \
                                              _mrkl4c_ctx->bsbufsz,            \
                                              "%.06lf [%d] %s %s[%d]: "        \
                                              context                          \
                                              mod ## _ ## msg ## _FMT,         \
                                              _mrkl4c_ctx->                    \
                                                writer.data.file.curtm,        \
                                              _mrkl4c_ctx->cache.pid,          \
                                              mod ## _NAME,                    \
                                              level_names[level],              \
                                              _mrkl4c_minfo->                  \
                                                nthrottled,                    \
                                              ##__VA_ARGS__);                  \
                if (_mrkl4c_nwritten < 0) {                                    \
                    bytestream_rewind(&_mrkl4c_ctx->bs);                       \
                } else {                                                       \
                    SADVANCEPOS(&_mrkl4c_ctx->bs, -1);                         \
                    (void)bytestream_cat(&_mrkl4c_ctx->bs, 1, "\n");           \
                    if (SEOD(&_mrkl4c_ctx->bs) >= _mrkl4c_ctx->bsbufsz) {      \
                        _mrkl4c_ctx->writer.write(_mrkl4c_ctx);                \
                    }                                                          \
                }                                                              \
                _mrkl4c_minfo->nthrottled = 0;                                 \
            } else {                                                           \
                ++_mrkl4c_minfo->nthrottled;                                   \
            }                                                                  \
        }                                                                      \
    } while (0)                                                                \


/*
 * once flevel
 */
#define MRKL4C_WRITE_ONCE_PRINTFLIKE_FLEVEL(ld, mod, msg, ...)                 \
    do {                                                                       \
        mrkl4c_ctx_t *_mrkl4c_ctx;                                             \
        mrkl4c_minfo_t *_mrkl4c_minfo;                                         \
        _mrkl4c_ctx = mrkl4c_get_ctx(ld);                                      \
        assert(_mrkl4c_ctx != NULL);                                           \
        _mrkl4c_minfo = ARRAY_GET(                                             \
            mrkl4c_minfo_t,                                                    \
            &_mrkl4c_ctx->minfos,                                              \
            mod ## _ ## msg ## _ID);                                           \
        if (mrkl4c_ctx_allowed(_mrkl4c_ctx,                                    \
                               _mrkl4c_minfo->flevel,                          \
                               mod ## _ ## msg ## _ID)) {                      \
            ssize_t _mrkl4c_nwritten;                                          \
            assert(_mrkl4c_ctx->writer.write != NULL);                         \
            _mrkl4c_ctx->writer.data.file.curtm = mrkl4c_now_posix();          \
            _mrkl4c_nwritten = bytestream_nprintf(&_mrkl4c_ctx->bs,            \
                                          _mrkl4c_ctx->bsbufsz,                \
                                          "%.06lf [%d] %s %s: "                \
                                          mod ## _ ## msg ## _FMT,             \
                                          _mrkl4c_ctx->                        \
                                            writer.data.file.curtm,            \
                                          _mrkl4c_ctx->cache.pid,              \
                                          mod ## _NAME,                        \
                                          level_names[_mrkl4c_minfo->flevel],  \
                                          ##__VA_ARGS__);                      \
            if (_mrkl4c_nwritten < 0) {                                        \
                bytestream_rewind(&_mrkl4c_ctx->bs);                           \
            } else {                                                           \
                SADVANCEPOS(&_mrkl4c_ctx->bs, -1);                             \
                (void)bytestream_cat(&_mrkl4c_ctx->bs, 1, "\n");               \
                _mrkl4c_ctx->writer.write(_mrkl4c_ctx);                        \
            }                                                                  \
        }                                                                      \
    } while (0)                                                                \


/*
 * once context flevel
 */
#define MRKL4C_WRITE_ONCE_PRINTFLIKE_CONTEXT_FLEVEL(ld, context, mod, msg, ...)\
    do {                                                                       \
        mrkl4c_ctx_t *_mrkl4c_ctx;                                             \
        mrkl4c_minfo_t *_mrkl4c_minfo;                                         \
        _mrkl4c_ctx = mrkl4c_get_ctx(ld);                                      \
        assert(_mrkl4c_ctx != NULL);                                           \
        _mrkl4c_minfo = ARRAY_GET(                                             \
            mrkl4c_minfo_t,                                                    \
            &_mrkl4c_ctx->minfos,                                              \
            mod ## _ ## msg ## _ID);                                           \
        if (mrkl4c_ctx_allowed(_mrkl4c_ctx,                                    \
                               _mrklc3_minfo->flevel,                          \
                               mod ## _ ## msg ## _ID)) {                      \
            ssize_t _mrkl4c_nwritten;                                          \
            assert(_mrkl4c_ctx->writer.write != NULL);                         \
            _mrkl4c_ctx->writer.data.file.curtm = mrkl4c_now_posix();          \
            _mrkl4c_nwritten = bytestream_nprintf(&_mrkl4c_ctx->bs,            \
                                          _mrkl4c_ctx->bsbufsz,                \
                                          "%.06lf [%d] %s %s: "                \
                                          context                              \
                                          mod ## _ ## msg ## _FMT,             \
                                          _mrkl4c_ctx->                        \
                                            writer.data.file.curtm,            \
                                          _mrkl4c_ctx->cache.pid,              \
                                          mod ## _NAME,                        \
                                          level_names[_mrkl4c_minfo->flevel],  \
                                          ##__VA_ARGS__);                      \
            if (_mrkl4c_nwritten < 0) {                                        \
                bytestream_rewind(&_mrkl4c_ctx->bs);                           \
            } else {                                                           \
                SADVANCEPOS(&_mrkl4c_ctx->bs, -1);                             \
                (void)bytestream_cat(&_mrkl4c_ctx->bs, 1, "\n");               \
                _mrkl4c_ctx->writer.write(_mrkl4c_ctx);                        \
            }                                                                  \
        }                                                                      \
    } while (0)                                                                \


/*
 * once
 */
#define MRKL4C_WRITE_ONCE_PRINTFLIKE(ld, level, mod, msg, ...)                 \
    do {                                                                       \
        mrkl4c_ctx_t *_mrkl4c_ctx;                                             \
        _mrkl4c_ctx = mrkl4c_get_ctx(ld);                                      \
        assert(_mrkl4c_ctx != NULL);                                           \
        if (mrkl4c_ctx_allowed(_mrkl4c_ctx, level, mod ## _ ## msg ## _ID)) {  \
            ssize_t _mrkl4c_nwritten;                                          \
            assert(_mrkl4c_ctx->writer.write != NULL);                         \
            _mrkl4c_ctx->writer.data.file.curtm = mrkl4c_now_posix();          \
            _mrkl4c_nwritten = bytestream_nprintf(&_mrkl4c_ctx->bs,            \
                                          _mrkl4c_ctx->bsbufsz,                \
                                          "%.06lf [%d] %s %s: "                \
                                          mod ## _ ## msg ## _FMT,             \
                                          _mrkl4c_ctx->                        \
                                            writer.data.file.curtm,            \
                                          _mrkl4c_ctx->cache.pid,              \
                                          mod ## _NAME,                        \
                                          level_names[level],                  \
                                          ##__VA_ARGS__);                      \
            if (_mrkl4c_nwritten < 0) {                                        \
                bytestream_rewind(&_mrkl4c_ctx->bs);                           \
            } else {                                                           \
                SADVANCEPOS(&_mrkl4c_ctx->bs, -1);                             \
                (void)bytestream_cat(&_mrkl4c_ctx->bs, 1, "\n");               \
                _mrkl4c_ctx->writer.write(_mrkl4c_ctx);                        \
            }                                                                  \
        }                                                                      \
    } while (0)                                                                \


/*
 * once context
 */
#define MRKL4C_WRITE_ONCE_PRINTFLIKE_CONTEXT(ld, level, context, mod, msg, ...)\
    do {                                                                       \
        mrkl4c_ctx_t *_mrkl4c_ctx;                                             \
        _mrkl4c_ctx = mrkl4c_get_ctx(ld);                                      \
        assert(_mrkl4c_ctx != NULL);                                           \
        if (mrkl4c_ctx_allowed(_mrkl4c_ctx, level, mod ## _ ## msg ## _ID)) {  \
            ssize_t _mrkl4c_nwritten;                                          \
            assert(_mrkl4c_ctx->writer.write != NULL);                         \
            _mrkl4c_ctx->writer.data.file.curtm = mrkl4c_now_posix();          \
            _mrkl4c_nwritten = bytestream_nprintf(&_mrkl4c_ctx->bs,            \
                                          _mrkl4c_ctx->bsbufsz,                \
                                          "%.06lf [%d] %s %s: "                \
                                          context                              \
                                          mod ## _ ## msg ## _FMT,             \
                                          _mrkl4c_ctx->                        \
                                            writer.data.file.curtm,            \
                                          _mrkl4c_ctx->cache.pid,              \
                                          mod ## _NAME,                        \
                                          level_names[level],                  \
                                          ##__VA_ARGS__);                      \
            if (_mrkl4c_nwritten < 0) {                                        \
                bytestream_rewind(&_mrkl4c_ctx->bs);                           \
            } else {                                                           \
                SADVANCEPOS(&_mrkl4c_ctx->bs, -1);                             \
                (void)bytestream_cat(&_mrkl4c_ctx->bs, 1, "\n");               \
                _mrkl4c_ctx->writer.write(_mrkl4c_ctx);                        \
            }                                                                  \
        }                                                                      \
    } while (0)                                                                \


/*
 * once lt
 */
#define MRKL4C_WRITE_ONCE_PRINTFLIKE_LT(ld, level, mod, msg, ...)              \
    do {                                                                       \
        mrkl4c_ctx_t *_mrkl4c_ctx;                                             \
        _mrkl4c_ctx = mrkl4c_get_ctx(ld);                                      \
        assert(_mrkl4c_ctx != NULL);                                           \
        if (mrkl4c_ctx_allowed(_mrkl4c_ctx, level, mod ## _ ## msg ## _ID)) {  \
            ssize_t _mrkl4c_nwritten;                                          \
            struct tm *_mrkl4c_tm;                                             \
            time_t _mtkl4c_now;                                                \
            char _mrkl4c_now_str[32];                                          \
            assert(_mrkl4c_ctx->writer.write != NULL);                         \
            _mrkl4c_ctx->writer.data.file.curtm = mrkl4c_now_posix();          \
            _mtkl4c_now = (time_t)_mrkl4c_ctx->writer.data.file.curtm;         \
            _mrkl4c_tm = localtime(&_mtkl4c_now);                              \
            (void)strftime(_mrkl4c_now_str,                                    \
                           sizeof(_mrkl4c_now_str),                            \
                           "%Y-%m-%d %H:%M:%S",                                \
                           _mrkl4c_tm);                                        \
            _mrkl4c_nwritten = bytestream_nprintf(&_mrkl4c_ctx->bs,            \
                                          _mrkl4c_ctx->bsbufsz,                \
                                          "%s [%d] %s %s: "                    \
                                          mod ## _ ## msg ## _FMT,             \
                                          _mrkl4c_now_str,                     \
                                          _mrkl4c_ctx->cache.pid,              \
                                          mod ## _NAME,                        \
                                          level_names[level],                  \
                                          ##__VA_ARGS__);                      \
            if (_mrkl4c_nwritten < 0) {                                        \
                bytestream_rewind(&_mrkl4c_ctx->bs);                           \
            } else {                                                           \
                SADVANCEPOS(&_mrkl4c_ctx->bs, -1);                             \
                (void)bytestream_cat(&_mrkl4c_ctx->bs, 1, "\n");               \
                _mrkl4c_ctx->writer.write(_mrkl4c_ctx);                        \
            }                                                                  \
        }                                                                      \
    } while (0)                                                                \


/*
 * once lt context
 */
#define MRKL4C_WRITE_ONCE_PRINTFLIKE_LT_CONTEXT(                               \
        ld, level, context, mod, msg, ...)                                     \
    do {                                                                       \
        mrkl4c_ctx_t *_mrkl4c_ctx;                                             \
        _mrkl4c_ctx = mrkl4c_get_ctx(ld);                                      \
        assert(_mrkl4c_ctx != NULL);                                           \
        if (mrkl4c_ctx_allowed(_mrkl4c_ctx, level, mod ## _ ## msg ## _ID)) {  \
            ssize_t _mrkl4c_nwritten;                                          \
            struct tm *_mrkl4c_tm;                                             \
            time_t _mtkl4c_now;                                                \
            char _mrkl4c_now_str[32];                                          \
            assert(_mrkl4c_ctx->writer.write != NULL);                         \
            _mrkl4c_ctx->writer.data.file.curtm = mrkl4c_now_posix();          \
            _mtkl4c_now = (time_t)_mrkl4c_ctx->writer.data.file.curtm;         \
            _mrkl4c_tm = localtime(&_mtkl4c_now);                              \
            (void)strftime(_mrkl4c_now_str,                                    \
                           sizeof(_mrkl4c_now_str),                            \
                           "%Y-%m-%d %H:%M:%S",                                \
                           _mrkl4c_tm);                                        \
            _mrkl4c_nwritten = bytestream_nprintf(&_mrkl4c_ctx->bs,            \
                                          _mrkl4c_ctx->bsbufsz,                \
                                          "%s [%d] %s %s: "                    \
                                          context                              \
                                          mod ## _ ## msg ## _FMT,             \
                                          _mrkl4c_now_str,                     \
                                          _mrkl4c_ctx->cache.pid,              \
                                          mod ## _NAME,                        \
                                          level_names[level],                  \
                                          ##__VA_ARGS__);                      \
            if (_mrkl4c_nwritten < 0) {                                        \
                bytestream_rewind(&_mrkl4c_ctx->bs);                           \
            } else {                                                           \
                SADVANCEPOS(&_mrkl4c_ctx->bs, -1);                             \
                (void)bytestream_cat(&_mrkl4c_ctx->bs, 1, "\n");               \
                _mrkl4c_ctx->writer.write(_mrkl4c_ctx);                        \
            }                                                                  \
        }                                                                      \
    } while (0)                                                                \


/*
 * once lt2
 */
#define MRKL4C_WRITE_ONCE_PRINTFLIKE_LT2(ld, level, mod, msg, ...)             \
    do {                                                                       \
        mrkl4c_ctx_t *_mrkl4c_ctx;                                             \
        _mrkl4c_ctx = mrkl4c_get_ctx(ld);                                      \
        assert(_mrkl4c_ctx != NULL);                                           \
        if (mrkl4c_ctx_allowed(_mrkl4c_ctx, level, mod ## _ ## msg ## _ID)) {  \
            ssize_t _mrkl4c_nwritten;                                          \
            struct tm *_mrkl4c_tm;                                             \
            time_t _mtkl4c_now;                                                \
            char _mrkl4c_now_str[32];                                          \
            assert(_mrkl4c_ctx->writer.write != NULL);                         \
            _mrkl4c_ctx->writer.data.file.curtm = mrkl4c_now_posix();          \
            _mtkl4c_now = (time_t)_mrkl4c_ctx->writer.data.file.curtm;         \
            _mrkl4c_tm = localtime(&_mtkl4c_now);                              \
            (void)strftime(_mrkl4c_now_str,                                    \
                           sizeof(_mrkl4c_now_str),                            \
                           "%Y-%m-%d %H:%M:%S",                                \
                           _mrkl4c_tm);                                        \
            _mrkl4c_nwritten = bytestream_nprintf(&_mrkl4c_ctx->bs,            \
                                          _mrkl4c_ctx->bsbufsz,                \
                                          "%lf %s [%d] %s %s: "                \
                                          mod ## _ ## msg ## _FMT,             \
                                          _mrkl4c_ctx->writer.data.file.curtm, \
                                          _mrkl4c_now_str,                     \
                                          _mrkl4c_ctx->cache.pid,              \
                                          mod ## _NAME,                        \
                                          level_names[level],                  \
                                          ##__VA_ARGS__);                      \
            if (_mrkl4c_nwritten < 0) {                                        \
                bytestream_rewind(&_mrkl4c_ctx->bs);                           \
            } else {                                                           \
                SADVANCEPOS(&_mrkl4c_ctx->bs, -1);                             \
                (void)bytestream_cat(&_mrkl4c_ctx->bs, 1, "\n");               \
                _mrkl4c_ctx->writer.write(_mrkl4c_ctx);                        \
            }                                                                  \
        }                                                                      \
    } while (0)                                                                \


/*
 * once lt2 context
 */
#define MRKL4C_WRITE_ONCE_PRINTFLIKE_LT2_CONTEXT(                              \
        ld, level, context, mod, msg, ...)                                     \
    do {                                                                       \
        mrkl4c_ctx_t *_mrkl4c_ctx;                                             \
        _mrkl4c_ctx = mrkl4c_get_ctx(ld);                                      \
        assert(_mrkl4c_ctx != NULL);                                           \
        if (mrkl4c_ctx_allowed(_mrkl4c_ctx, level, mod ## _ ## msg ## _ID)) {  \
            ssize_t _mrkl4c_nwritten;                                          \
            struct tm *_mrkl4c_tm;                                             \
            time_t _mtkl4c_now;                                                \
            char _mrkl4c_now_str[32];                                          \
            assert(_mrkl4c_ctx->writer.write != NULL);                         \
            _mrkl4c_ctx->writer.data.file.curtm = mrkl4c_now_posix();          \
            _mtkl4c_now = (time_t)_mrkl4c_ctx->writer.data.file.curtm;         \
            _mrkl4c_tm = localtime(&_mtkl4c_now);                              \
            (void)strftime(_mrkl4c_now_str,                                    \
                           sizeof(_mrkl4c_now_str),                            \
                           "%Y-%m-%d %H:%M:%S",                                \
                           _mrkl4c_tm);                                        \
            _mrkl4c_nwritten = bytestream_nprintf(&_mrkl4c_ctx->bs,            \
                                          _mrkl4c_ctx->bsbufsz,                \
                                          "%lf %s [%d] %s %s: "                \
                                          context                              \
                                          mod ## _ ## msg ## _FMT,             \
                                          _mrkl4c_ctx->writer.data.file.curtm, \
                                          _mrkl4c_now_str,                     \
                                          _mrkl4c_ctx->cache.pid,              \
                                          mod ## _NAME,                        \
                                          level_names[level],                  \
                                          ##__VA_ARGS__);                      \
            if (_mrkl4c_nwritten < 0) {                                        \
                bytestream_rewind(&_mrkl4c_ctx->bs);                           \
            } else {                                                           \
                SADVANCEPOS(&_mrkl4c_ctx->bs, -1);                             \
                (void)bytestream_cat(&_mrkl4c_ctx->bs, 1, "\n");               \
                _mrkl4c_ctx->writer.write(_mrkl4c_ctx);                        \
            }                                                                  \
        }                                                                      \
    } while (0)                                                                \


/*
 * start
 */
#define MRKL4C_WRITE_START_PRINTFLIKE(ld, level, mod, msg, ...)                \
    do {                                                                       \
        mrkl4c_ctx_t *_mrkl4c_ctx;                                             \
        _mrkl4c_ctx = mrkl4c_get_ctx(ld);                                      \
        assert(_mrkl4c_ctx != NULL);                                           \
        if (mrkl4c_ctx_allowed(_mrkl4c_ctx, level, mod ## _ ## msg ## _ID)) {  \
            ssize_t _mrkl4c_nwritten;                                          \
            _mrkl4c_ctx->writer.data.file.curtm = mrkl4c_now_posix();          \
            _mrkl4c_nwritten = bytestream_nprintf(&_mrkl4c_ctx->bs,            \
                                          _mrkl4c_ctx->bsbufsz,                \
                                          "%.06lf [%d] %s %s: "                \
                                          mod ## _ ## msg ## _FMT,             \
                                          _mrkl4c_ctx->                        \
                                            writer.data.file.curtm,            \
                                          _mrkl4c_ctx->cache.pid,              \
                                          mod ## _NAME,                        \
                                          level_names[level],                  \
                                          ##__VA_ARGS__);                      \


/*
 * start context
 */
#define MRKL4C_WRITE_START_PRINTFLIKE_CONTEXT(                                 \
        ld, level, context, mod, msg, ...)                                     \
    do {                                                                       \
        mrkl4c_ctx_t *_mrkl4c_ctx;                                             \
        _mrkl4c_ctx = mrkl4c_get_ctx(ld);                                      \
        assert(_mrkl4c_ctx != NULL);                                           \
        if (mrkl4c_ctx_allowed(_mrkl4c_ctx, level, mod ## _ ## msg ## _ID)) {  \
            ssize_t _mrkl4c_nwritten;                                          \
            _mrkl4c_ctx->writer.data.file.curtm = mrkl4c_now_posix();          \
            _mrkl4c_nwritten = bytestream_nprintf(&_mrkl4c_ctx->bs,            \
                                          _mrkl4c_ctx->bsbufsz,                \
                                          "%.06lf [%d] %s %s: "                \
                                          context                              \
                                          mod ## _ ## msg ## _FMT,             \
                                          _mrkl4c_ctx->                        \
                                            writer.data.file.curtm,            \
                                          _mrkl4c_ctx->cache.pid,              \
                                          mod ## _NAME,                        \
                                          level_names[level],                  \
                                          ##__VA_ARGS__);                      \


/*
 * start lt
 */
#define MRKL4C_WRITE_START_PRINTFLIKE_LT(ld, level, mod, msg, ...)             \
    do {                                                                       \
        mrkl4c_ctx_t *_mrkl4c_ctx;                                             \
        _mrkl4c_ctx = mrkl4c_get_ctx(ld);                                      \
        assert(_mrkl4c_ctx != NULL);                                           \
        if (mrkl4c_ctx_allowed(_mrkl4c_ctx, level, mod ## _ ## msg ## _ID)) {  \
            ssize_t _mrkl4c_nwritten;                                          \
            struct tm *_mrkl4c_tm;                                             \
            time_t _mtkl4c_now;                                                \
            char _mrkl4c_now_str[32];                                          \
            _mrkl4c_ctx->writer.data.file.curtm = mrkl4c_now_posix();          \
            _mtkl4c_now = (time_t)_mrkl4c_ctx->writer.data.file.curtm;         \
            _mrkl4c_tm = localtime(&_mtkl4c_now);                              \
            (void)strftime(_mrkl4c_now_str,                                    \
                           sizeof(_mrkl4c_now_str),                            \
                           "%Y-%m-%d %H:%M:%S",                                \
                           _mrkl4c_tm);                                        \
            _mrkl4c_nwritten = bytestream_nprintf(&_mrkl4c_ctx->bs,            \
                                          _mrkl4c_ctx->bsbufsz,                \
                                          "%s [%d] %s %s: "                    \
                                          mod ## _ ## msg ## _FMT,             \
                                          _mrkl4c_now_str,                     \
                                          _mrkl4c_ctx->cache.pid,              \
                                          mod ## _NAME,                        \
                                          level_names[level],                  \
                                          ##__VA_ARGS__);                      \


/*
 * start lt context
 */
#define MRKL4C_WRITE_START_PRINTFLIKE_LT_CONTEXT(                              \
        ld, level, context, mod, msg, ...)                                     \
    do {                                                                       \
        mrkl4c_ctx_t *_mrkl4c_ctx;                                             \
        _mrkl4c_ctx = mrkl4c_get_ctx(ld);                                      \
        assert(_mrkl4c_ctx != NULL);                                           \
        if (mrkl4c_ctx_allowed(_mrkl4c_ctx, level, mod ## _ ## msg ## _ID)) {  \
            ssize_t _mrkl4c_nwritten;                                          \
            struct tm *_mrkl4c_tm;                                             \
            time_t _mtkl4c_now;                                                \
            char _mrkl4c_now_str[32];                                          \
            _mrkl4c_ctx->writer.data.file.curtm = mrkl4c_now_posix();          \
            _mtkl4c_now = (time_t)_mrkl4c_ctx->writer.data.file.curtm;         \
            _mrkl4c_tm = localtime(&_mtkl4c_now);                              \
            (void)strftime(_mrkl4c_now_str,                                    \
                           sizeof(_mrkl4c_now_str),                            \
                           "%Y-%m-%d %H:%M:%S",                                \
                           _mrkl4c_tm);                                        \
            _mrkl4c_nwritten = bytestream_nprintf(&_mrkl4c_ctx->bs,            \
                                          _mrkl4c_ctx->bsbufsz,                \
                                          "%s [%d] %s %s: "                    \
                                          context                              \
                                          mod ## _ ## msg ## _FMT,             \
                                          _mrkl4c_now_str,                     \
                                          _mrkl4c_ctx->cache.pid,              \
                                          mod ## _NAME,                        \
                                          level_names[level],                  \
                                          ##__VA_ARGS__);                      \


/*
 * start lt2
 */
#define MRKL4C_WRITE_START_PRINTFLIKE_LT2(ld, level, mod, msg, ...)            \
    do {                                                                       \
        mrkl4c_ctx_t *_mrkl4c_ctx;                                             \
        _mrkl4c_ctx = mrkl4c_get_ctx(ld);                                      \
        assert(_mrkl4c_ctx != NULL);                                           \
        if (mrkl4c_ctx_allowed(_mrkl4c_ctx, level, mod ## _ ## msg ## _ID)) {  \
            ssize_t _mrkl4c_nwritten;                                          \
            struct tm *_mrkl4c_tm;                                             \
            time_t _mtkl4c_now;                                                \
            char _mrkl4c_now_str[32];                                          \
            _mrkl4c_ctx->writer.data.file.curtm = mrkl4c_now_posix();          \
            _mtkl4c_now = (time_t)_mrkl4c_ctx->writer.data.file.curtm;         \
            _mrkl4c_tm = localtime(&_mtkl4c_now);                              \
            (void)strftime(_mrkl4c_now_str,                                    \
                           sizeof(_mrkl4c_now_str),                            \
                           "%Y-%m-%d %H:%M:%S",                                \
                           _mrkl4c_tm);                                        \
            _mrkl4c_nwritten = bytestream_nprintf(&_mrkl4c_ctx->bs,            \
                                          _mrkl4c_ctx->bsbufsz,                \
                                          "%lf %s [%d] %s %s: "                \
                                          mod ## _ ## msg ## _FMT,             \
                                          _mrkl4c_ctx->writer.data.file.curtm, \
                                          _mrkl4c_now_str,                     \
                                          _mrkl4c_ctx->cache.pid,              \
                                          mod ## _NAME,                        \
                                          level_names[level],                  \
                                          ##__VA_ARGS__);                      \


/*
 * start lt2 context
 */
#define MRKL4C_WRITE_START_PRINTFLIKE_LT2_CONTEXT(                             \
        ld, level, context, mod, msg, ...)                                     \
    do {                                                                       \
        mrkl4c_ctx_t *_mrkl4c_ctx;                                             \
        _mrkl4c_ctx = mrkl4c_get_ctx(ld);                                      \
        assert(_mrkl4c_ctx != NULL);                                           \
        if (mrkl4c_ctx_allowed(_mrkl4c_ctx, level, mod ## _ ## msg ## _ID)) {  \
            ssize_t _mrkl4c_nwritten;                                          \
            struct tm *_mrkl4c_tm;                                             \
            time_t _mtkl4c_now;                                                \
            char _mrkl4c_now_str[32];                                          \
            _mrkl4c_ctx->writer.data.file.curtm = mrkl4c_now_posix();          \
            _mtkl4c_now = (time_t)_mrkl4c_ctx->writer.data.file.curtm;         \
            _mrkl4c_tm = localtime(&_mtkl4c_now);                              \
            (void)strftime(_mrkl4c_now_str,                                    \
                           sizeof(_mrkl4c_now_str),                            \
                           "%Y-%m-%d %H:%M:%S",                                \
                           _mrkl4c_tm);                                        \
            _mrkl4c_nwritten = bytestream_nprintf(&_mrkl4c_ctx->bs,            \
                                          _mrkl4c_ctx->bsbufsz,                \
                                          "%lf %s [%d] %s %s: "                \
                                          context                              \
                                          mod ## _ ## msg ## _FMT,             \
                                          _mrkl4c_ctx->writer.data.file.curtm, \
                                          _mrkl4c_now_str,                     \
                                          _mrkl4c_ctx->cache.pid,              \
                                          mod ## _NAME,                        \
                                          level_names[level],                  \
                                          ##__VA_ARGS__);                      \


/*
 * next
 */
#define MRKL4C_WRITE_NEXT_PRINTFLIKE(ld, level, mod, msg, fmt, ...)    \
            _mrkl4c_nwritten = bytestream_nprintf(&_mrkl4c_ctx->bs,    \
                                     _mrkl4c_ctx->bsbufsz,             \
                                     fmt,                              \
                                     ##__VA_ARGS__)                    \


/*
 * next context
 */
#define MRKL4C_WRITE_NEXT_PRINTFLIKE_CONTEXT(                          \
        ld, level, context, mod, msg, fmt, ...)                        \
            _mrkl4c_nwritten = bytestream_nprintf(&_mrkl4c_ctx->bs,    \
                                     _mrkl4c_ctx->bsbufsz,             \
                                     context                           \
                                     fmt,                              \
                                     ##__VA_ARGS__)                    \


/*
 * stop
 */
#define MRKL4C_WRITE_STOP_PRINTFLIKE(ld, level, mod, msg, ...)                 \
            if (_mrkl4c_nwritten < 0) {                                        \
                bytestream_rewind(&_mrkl4c_ctx->bs);                           \
            } else {                                                           \
                SADVANCEPOS(&_mrkl4c_ctx->bs, -1);                             \
                (void)bytestream_nprintf(&_mrkl4c_ctx->bs,                     \
                                         _mrkl4c_ctx->bsbufsz,                 \
                                         mod ## _ ## msg ## _FMT,              \
                                         ##__VA_ARGS__);                       \
                (void)bytestream_cat(&_mrkl4c_ctx->bs, 1, "\n");               \
                _mrkl4c_ctx->writer.write(_mrkl4c_ctx);                        \
            }                                                                  \
        }                                                                      \
    } while (0)                                                                \



/*
 * stop context
 */
#define MRKL4C_WRITE_STOP_PRINTFLIKE_CONTEXT(                                  \
        ld, level, context, mod, msg, ...)                                     \
            if (_mrkl4c_nwritten < 0) {                                        \
                bytestream_rewind(&_mrkl4c_ctx->bs);                           \
            } else {                                                           \
                SADVANCEPOS(&_mrkl4c_ctx->bs, -1);                             \
                (void)bytestream_nprintf(&_mrkl4c_ctx->bs,                     \
                                         _mrkl4c_ctx->bsbufsz,                 \
                                         context                               \
                                         mod ## _ ## msg ## _FMT,              \
                                         ##__VA_ARGS__);                       \
                (void)bytestream_cat(&_mrkl4c_ctx->bs, 1, "\n");               \
                _mrkl4c_ctx->writer.write(_mrkl4c_ctx);                        \
            }                                                                  \
        }                                                                      \
    } while (0)                                                                \



/*
 * do at
 */
#define MRKL4C_DO_AT(ld, level, mod, msg, __a1)                                \
    do {                                                                       \
        mrkl4c_ctx_t *_mrkl4c_ctx;                                             \
        _mrkl4c_ctx = mrkl4c_get_ctx(ld);                                      \
        assert(_mrkl4c_ctx != NULL);                                           \
        if (mrkl4c_ctx_allowed(_mrkl4c_ctx, level, mod ## _ ## msg ## _ID)) {  \
            __a1                                                               \
        }                                                                      \
    } while (0)                                                                \



#ifdef __cplusplus
}
#endif
#endif /* MRKL4C_H_DEFINED */
