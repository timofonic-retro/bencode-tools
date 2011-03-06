#include <typevalidator/bencode.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

static size_t find(const char *data, size_t len, size_t off, char c)
{
	for (; off < len; off++) {
		if (data[off] == c)
			return off;
	}
	return -1;
}

static struct bencode *alloc(enum bencodetype type)
{
	struct bencode *b = calloc(1, sizeof *b);
	if (b == NULL)
		return NULL;
	b->type = type;
	return b;
}

static struct bencode *overflow(size_t off)
{
	fprintf(stderr, "bencode: overflow at position %zu\n", off);
	return NULL;
}

static struct bencode *invalid(const char *reason, size_t off)
{
	fprintf(stderr, "bencode: %s: invalid data at position %zu\n", reason, off);
	return NULL;
}

static struct bencode *decode_int(const char *data, size_t len, size_t *off)
{
	/* fits all 64 bit integers */
	char buf[21];
	size_t slen;
	struct bencode *b;
	char *endptr;
	size_t pos;
	long long ll;

	pos = find(data, len, *off + 1, 'e');
	if (pos == -1)
		return overflow(*off);
	slen = pos - *off - 1;
	if (slen == 0 || slen >= sizeof buf)
		return invalid("bad int slen", *off);
	assert(slen < sizeof buf);
	memcpy(buf, data + *off + 1, slen);
	buf[slen] = 0;
	errno = 0;
	ll = strtoll(buf, &endptr, 10);
	if (errno == ERANGE || *endptr != 0)
		return invalid("bad int string", *off);
	b = alloc(BENCODE_INT);
	if (b == NULL) {
		fprintf(stderr, "bencode: No memory for int\n");
		return NULL;
	}
	b->ll = ll;
	*off = pos;
	return b;
}

static size_t read_size_t(const char *buf)
{
	char *endptr;
	long long ll;
	size_t s;

	errno = 0;
	/* Note: value equal to ((size_t) -1) is not valid */
	ll = strtoll(buf, &endptr, 10);
	if (errno == ERANGE || *endptr != 0)
		return -1;
	if (ll < 0)
		return -1;
	s = (size_t) ll;
	if (ll != (long long) s)
		return -1;
	return s;
}

static struct bencode *decode_str(const char *data, size_t len, size_t *off)
{
	char buf[21];
	size_t pos;
	size_t slen;
	size_t datalen;
	struct bencode *b;
	size_t newoff;

	pos = find(data, len, *off + 1, ':');
	if (pos == -1)
		return overflow(*off);
	slen = pos - *off;
	if (slen == 0 || slen >= sizeof buf)
		return invalid("no string length", *off);
	assert(slen < sizeof buf);
	memcpy(buf, data + *off, slen);
	buf[slen] = 0;

	/* Read the string length */
	datalen = read_size_t(buf);
	if (datalen == -1)
		return invalid("invalid string length", *off);

	newoff = pos + 1 + datalen;
	if (newoff > len)
		return invalid("too long a string (data out of bounds)", *off);

	/* Allocate string structure and copy data into it */
	b = alloc(BENCODE_STR);
	if (b == NULL) {
		fprintf(stderr, "bencode: No memory for str structure\n");
		return NULL;
	}	
	b->s.s = malloc(datalen);
	if (b->s.s == NULL) {
		free(b);
		fprintf(stderr, "bencode: No memory for string\n");
		return NULL;
	}
	memcpy(b->s.s, data + pos + 1, datalen);
	b->s.len = datalen;

	*off = newoff;
	return b;
}

static struct bencode *decode(const char *data, size_t len, size_t *off, int l)
{
	l++;
	if (l > 256)
		return NULL;
	if (*off == len)
		return NULL;
	assert (*off < len);
	switch (data[*off]) {
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		return decode_str(data, len, off);
	case 'i':
		return decode_int(data, len, off);
	default:
		return invalid("unknown bencode type", *off);
	}
}

struct bencode *ben_decode(const void *data, size_t len)
{
	size_t off = 0;
	return decode((const char *) data, len, &off, 0);
}

struct bencode *ben_decode2(const void *data, size_t len, size_t *off)
{
	return decode((const char *) data, len, off, 0);
}

void ben_free(struct bencode *b)
{
	if (b == NULL)
		return;
	switch (b->type) {
	case BENCODE_INT:
		break;
	case BENCODE_STR:
		free(b->s.s);
		b->s.s = NULL;
		b->s.len = 0;
		break;
	default:
		fprintf(stderr, "bencode: invalid type: %d\n", b->type);
		exit(1);
	}
	b->type = 0;
	free(b);
}