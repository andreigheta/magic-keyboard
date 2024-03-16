#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#define DIE(assertion, call_description)				\
	do {								\
		if (assertion) {					\
			fprintf(stderr, "(%s, %d): ",			\
					__FILE__, __LINE__);		\
			perror(call_description);			\
			exit(errno);					\
		}							\
	} while (0)

#define ALPHABET_SIZE 26
#define ALPHABET "abcdefghijklmnopqrstuvwxyz"
#define NMAX 50

typedef struct trie_node_t trie_node_t;
struct trie_node_t {
	/* Value associated with key (set if end_of_word = 1) */
	int freq;

	/* 1 if current node marks the end of a word, 0 otherwise */
	int end_of_word;

	trie_node_t **children;
	int n_children;
};

typedef struct trie_t trie_t;
struct trie_t {
	trie_node_t *root;
	int size;
	int data_size;
	int alphabet_size;

	char *alphabet;
	void (*free_value_cb)(void *value);

	int nr_nodes;
};

trie_node_t *trie_create_node(trie_t *trie)
{
	trie_node_t *node = (trie_node_t *)malloc(sizeof(trie_node_t));
	DIE(!node, "Malloc failed");

	node->freq = 0;
	node->end_of_word = 0;
	node->n_children = 0;

	node->children = calloc(trie->alphabet_size, sizeof(trie_node_t *));
	DIE(!node->children, "Calloc failed");

	for (int i = 0; i < trie->alphabet_size; i++)
		node->children[i] = NULL;

	return node;
}

trie_t *trie_create(int data_size, int alphabet_size, char *alphabet,
					void (*free_value_cb)(void *))
{
	trie_t *trie = (trie_t *)malloc(sizeof(trie_t));
	DIE(!trie, "Malloc failed");

	trie->data_size = data_size;
	trie->alphabet = alphabet;
	trie->alphabet_size = alphabet_size;
	trie->root = trie_create_node(trie);
	trie->free_value_cb = free_value_cb;
	trie->nr_nodes = 1;

	return trie;
}

void trie_insert(trie_t *trie, trie_node_t *node, char *key)
{
	if (strlen(key) == 0) {
		node->freq++;
		node->end_of_word = 1;

		return;
	}

	trie_node_t *next_node = node->children[key[0] - 'a'];

	if (!next_node) {
		next_node = trie_create_node(trie);
		node->n_children++;
		node->children[key[0] - 'a'] = next_node;
		trie->nr_nodes++;
	}

	trie_insert(trie, next_node, key + 1);
}

int trie_search(trie_t *trie, trie_node_t *node, char *key)
{
	if (strlen(key) == 0) {
		if (!node->end_of_word)
			return 0;
		return node->freq;
	}

	trie_node_t *next_node = node->children[key[0] - 'a'];

	if (!next_node)
		return 0;

	return trie_search(trie, next_node, key + 1);
}

void trie_free(trie_node_t *node)
{
	for (int i = 0; i < ALPHABET_SIZE; i++) {
		if (node->children[i])
			trie_free(node->children[i]);
	}

	free(node->children);
	free(node);
}

int trie_remove(trie_t *trie, trie_node_t *node, char *key)
{
	if (strlen(key) == 0) {
		node->freq = 0;

		if (node->end_of_word) {
			node->end_of_word = 0;
			return node->n_children == 0;
		}
		return 0;
	}

	trie_node_t *next_node = node->children[key[0] - 'a'];

	if (next_node && trie_remove(trie, next_node, key + 1) == 1) {
		node->freq = 0;
		for (int i = 0; i < ALPHABET_SIZE; i++) {
			if (next_node->children[i]) {
				free(next_node->children[i]);
				next_node->children[i] = NULL;
			}
		}
		free(next_node->children);
		free(next_node);

		node->children[key[0] - 'a'] = NULL;
		node->n_children--;
		trie->nr_nodes--;

		if (node->n_children == 0 && node->end_of_word == 0)
			return 1;

		return 0;
	}
}

void load(trie_t *trie, char filename[])
{
	FILE *file = fopen(filename, "rt");
	DIE(!file, "Couldn't open file");

	char word[NMAX];
	while (fscanf(file, "%s", word) != EOF)
		trie_insert(trie, trie->root, word);

	fclose(file);
}

void autocorrect(trie_node_t *node, char word[], int k, char *prefix,
				 int *nr_words)
{
	if (!node)
		return;

	if (node->end_of_word == 1) {
		char *p = word;
		char *q = prefix;

		int len_word = strlen(word);
		int len_prefix = strlen(prefix);

		int not_matches = 0;
		if (len_word == len_prefix) {
			while (len_word != 0) {
				if (*p != *q)
					not_matches++;

				len_prefix--;
				len_word--;
				p++;
				q++;
			}

			if (not_matches <= k) {
				printf("%s\n", prefix);
				(*nr_words)++;
			}
		}
	}

	for (int i = 0; i < ALPHABET_SIZE; i++) {
		if (node->children[i]) {
			char ch = 'a' + i;

			char *new_prefix =
			(char *)malloc(sizeof(char) * (strlen(prefix) + 2));
			DIE(!new_prefix, "Malloc failed");

			strcpy(new_prefix, prefix);

			int len = strlen(new_prefix);
			new_prefix[len] = ch;
			new_prefix[len + 1] = '\0';

			autocorrect(node->children[i], word, k, new_prefix, nr_words);
			free(new_prefix);
		}
	}
}

void case1(char word[], char *prefix, char *best_match)
{
	char *p = word;
	char *q = prefix;

	int len_prefix = strlen(prefix);
	int len_word = strlen(word);

	int matches = 0;
	while (len_word != 0) {
		if (*p == *q)
			matches++;

		len_word--;
		p++;
		q++;
	}

	len_word = strlen(word);

	if (matches == len_word && strcmp(prefix, best_match) < 0)
		strcpy(best_match, prefix);
}

void case2(char word[], char *prefix, char *best_match)
{
	char *p = word;
	char *q = prefix;

	int len_prefix = strlen(prefix);
	int len_word = strlen(word);

	int matches = 0;
	while (len_word != 0) {
		if (*p == *q)
			matches++;

		len_word--;
		p++;
		q++;
	}

	len_word = strlen(word);
	int len_best_match = strlen(best_match);

	if (matches == len_word && len_prefix < len_best_match)
		strcpy(best_match, prefix);
}

void case3(trie_node_t *node, char word[], char *prefix, char *best_match,
		   int *freq_best_match)
{
	char *p = word;
	char *q = prefix;

	int len_prefix = strlen(prefix);
	int len_word = strlen(word);

	int matches = 0;
	while (len_word != 0) {
		if (*p == *q)
			matches++;

		len_word--;
		p++;
		q++;
	}

	len_word = strlen(word);

	int freq_prefix = node->freq;

	if (matches == len_word && freq_prefix > *freq_best_match) {
		strcpy(best_match, prefix);
		*freq_best_match = freq_prefix;
	}

	if (matches == len_word && freq_prefix == *freq_best_match)
		if (strcmp(prefix, best_match) < 0)
			strcpy(best_match, prefix);
}

void autocomplete(trie_node_t *node, char word[], int k, char *prefix,
				  char *best_match, int *freq_best_match)
{
	if (!node)
		return;

	if (node->end_of_word == 1) {
		if (k == 1)
			case1(word, prefix, best_match);

		if (k == 2)
			case2(word, prefix, best_match);

		if (k == 3)
			case3(node, word, prefix, best_match, freq_best_match);
	}

	for (int i = 0; i < ALPHABET_SIZE; i++) {
		if (node->children[i]) {
			char ch = 'a' + i;

			char *new_prefix =
			(char *)malloc(sizeof(char) * (strlen(prefix) + 2));
			DIE(!new_prefix, "Malloc failed");

			strcpy(new_prefix, prefix);

			int len = strlen(new_prefix);
			new_prefix[len] = ch;
			new_prefix[len + 1] = '\0';

			autocomplete(node->children[i], word, k, new_prefix, best_match,
						 freq_best_match);
			free(new_prefix);
		}
	}
}

int main(void)
{
	int k;
	char command[NMAX], word[NMAX];
	char alphabet[] = ALPHABET;
	trie_t *trie = trie_create(sizeof(int), ALPHABET_SIZE, alphabet, free);

	scanf("%s", command);

	while (1) {
		if (strcmp(command, "LOAD") == 0) {
			char filename[NMAX];
			scanf("%s", filename);
			load(trie, filename);
		} else if (strcmp(command, "INSERT") == 0) {
			scanf("%s", word);
			trie_insert(trie, trie->root, word);
		} else if (strcmp(command, "REMOVE") == 0) {
			scanf("%s", word);
			trie_remove(trie, trie->root, word);
		} else if (strcmp(command, "AUTOCORRECT") == 0) {
			scanf("%s", word);
			scanf("%d", &k);
			int nr_words = 0;
			autocorrect(trie->root, word, k, "", &nr_words);
			if (nr_words == 0)
				printf("No words found\n");
		} else if (strcmp(command, "AUTOCOMPLETE") == 0) {
			scanf("%s", word);
			scanf("%d", &k);
			char best_match[NMAX] = "zzzzzzzzzzzzzzzzzzzzz";
			int freq_best_match = -1;
			if (k == 0) {
				autocomplete(trie->root, word, 1, "", best_match,
							 &freq_best_match);
				if (strcmp(best_match, "zzzzzzzzzzzzzzzzzzzzz") == 0)
					printf("No words found\n");
				else
					printf("%s\n", best_match);

				autocomplete(trie->root, word, 2, "", best_match,
							 &freq_best_match);
				if (strcmp(best_match, "zzzzzzzzzzzzzzzzzzzzz") == 0)
					printf("No words found\n");
				else
					printf("%s\n", best_match);

				autocomplete(trie->root, word, 3, "", best_match,
							 &freq_best_match);
				if (strcmp(best_match, "zzzzzzzzzzzzzzzzzzzzz") == 0)
					printf("No words found\n");
				else
					printf("%s\n", best_match);
			} else {
				autocomplete(trie->root, word, k, "", best_match,
							 &freq_best_match);
				if (strcmp(best_match, "zzzzzzzzzzzzzzzzzzzzz") == 0)
					printf("No words found\n");
				else
					printf("%s\n", best_match);
			}
		} else if (strcmp(command, "EXIT") == 0) {
			trie_free(trie->root);
			free(trie);
			break;
		}

		scanf("%s", command);
	}
	return 0;
}
