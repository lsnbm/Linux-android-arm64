#include "../../arm64_decode/arm64_decode.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <instruction.txt>\n", argv[0]);
        return EXIT_FAILURE;
    }

    FILE *input = fopen(argv[1], "r");

    if (!input)
    {
        perror(argv[1]);
        return EXIT_FAILURE;
    }

    printf("index\traw\tstatus\tclass\tinstruction\toffset\timmediate\tbitfield_wmask\tbitfield_tmask\textend_type\tshift_type\tshift_amount\timmr\timms\tsimd_cmode\tcondition\tnzcv\tsysreg\telement_width\tlane_index\trd\trn\trm\tra\trt\trt2\trs\toperand_width\n");

    char line[64];
    size_t index = 0;
    size_t line_number = 0;
    unsigned int errors = 0;
    unsigned int rejected = 0;

    while (fgets(line, sizeof(line), input))
    {
        line_number++;
        char *line_end = line + strlen(line);

         while (line_end > line &&
             (line_end[-1] == '\n' || line_end[-1] == '\r'))
            *--line_end = '\0';
        if (*line == '\0')
            continue;

        char *end;
        unsigned long value = strtoul(line, &end, 16);

        if (line_end - line != 8 ||
            strspn(line, "0123456789abcdefABCDEF") != 8 ||
            *end != '\0' || value > UINT32_MAX)
        {
            fprintf(stderr, "%s:%zu: expected exactly eight hex digits, got '%s'\n",
                    argv[1], line_number, line);
                errors++;
            index++;
            continue;
        }
        uint32_t raw = (uint32_t)value;
        struct arm64_decoded_instruction decoded = { 0 };
        enum arm64_decode_status status = arm64_decode_instruction(raw, &decoded);

        if (status != ARM64_DECODE_OK)
            rejected++;
        printf("%zu\t0x%08" PRIx32 "\t%u\t%u\t%u\t%" PRId64 "\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64,
               index, raw, (unsigned int)status,
               (unsigned int)decoded.instruction_class,
               (unsigned int)decoded.instruction, decoded.offset,
               decoded.immediate, decoded.bitfield_wmask,
               decoded.bitfield_tmask);
        printf("\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\n",
               decoded.extend_type, decoded.shift_type, decoded.shift_amount,
               decoded.immr, decoded.imms, decoded.simd_cmode,
               decoded.condition, decoded.nzcv, decoded.sysreg,
               decoded.element_width, decoded.lane_index, decoded.rd,
               decoded.rn, decoded.rm, decoded.ra, decoded.rt, decoded.rt2,
               decoded.rs, decoded.operand_width);
        index++;
    }

    if (ferror(input))
    {
        perror(argv[1]);
        errors++;
    }
    fclose(input);
    fprintf(stderr, "ARM64 instruction decoder: rows=%zu rejected=%u errors=%u\n",
            index, rejected, errors);
    return errors ? EXIT_FAILURE : EXIT_SUCCESS;
}
