#ifndef __ROCKX_BM25_H__
#define __ROCKX_BM25_H__

#include "rockx.h"
#include "rockx_type.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RockXBM25InitParam {
    char* model_file;           // Path to BM25 model file
    char* stopwords_file;       // Path to stopwords file
    char* corpus_file;          // Path to corpus file (optional)
    float k1;                   // BM25 parameter k1
    float b;                    // BM25 parameter b
    int is_chinese;
    int from_db;                // Whether to load from database
} RockXBM25InitParam;

typedef struct RockXBM25QueryParam {
    char* query_text;           // Query text
    int is_chinese;             // Whether to use Chinese tokenizer
    int topn;                   // Number of results to return
} RockXBM25QueryParam;

typedef struct RockXBM25Result {
    int* doc_ids;               // Array of document IDs
    float* scores;              // Array of scores
    int count;                  // Number of results
} RockXBM25Result;

#ifdef __cplusplus
}
#endif

#endif