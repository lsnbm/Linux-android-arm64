/*
 * ARM64 解码器测试运行器。
 *
 * 本文件会与生产解码器 arm64_decode_*.c 一起编译，运行时读取输入文件
 * 中的 ARM64 原始机器码，逐条调用 arm64_decode_instruction()，并将完整
 * 的解码结果以 TSV 格式输出，供独立审计工具进行严格校验。
 */
#include "../../arm64_decode/arm64_decode.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int parse_raw(const char *line, uint32_t *raw)
{
    char text[9];
    char *end;
    size_t length = strlen(line);

    if (length != 8) return 0;
    memcpy(text, line, 8);
    text[8] = '\0';
    for (size_t index = 0; index < 8; index++)
    {
        char character = text[index];

        if (!((character >= '0' && character <= '9') ||
              (character >= 'A' && character <= 'F') ||
              (character >= 'a' && character <= 'f'))) return 0;
    }

    unsigned long value = strtoul(text, &end, 16);

    if (*end != '\0' || value > UINT32_MAX) return 0;
    *raw = (uint32_t)value;
    return 1;
}

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
    unsigned int failures = 0;

    while (fgets(line, sizeof(line), input))
    {
        struct arm64_decoded_instruction decoded;
        uint32_t raw;

        {
            char *line_end = line + strlen(line);
            while (line_end > line && (line_end[-1] == '\n' || line_end[-1] == '\r' || line_end[-1] == ' ' || line_end[-1] == '\t'))
                *--line_end = '\0';
            if (*line == '\0') continue;
        }

        if (!parse_raw(line, &raw))
        {
                fprintf(stderr, "%s:%zu: invalid instruction line length=%zu text='%s'\n",
                    argv[1], index + 1, strlen(line), line);
            failures++;
            index++;
            continue;
        }

         enum arm64_decode_status status = arm64_decode_instruction(raw, &decoded);

         if (status != ARM64_DECODE_OK)
         {
             failures++;
             fprintf(stderr, "decode failure: index=%zu raw=0x%08" PRIx32 " status=%u\n",
                     index, raw, (unsigned int)status);
         }
           printf("%zu\t0x%08" PRIx32 "\t%u\t%u\t%u\t%" PRId64 "\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64,
             index, raw, (unsigned int)status, (unsigned int)decoded.instruction_class,
               (unsigned int)decoded.instruction, decoded.offset, decoded.immediate,
               decoded.bitfield_wmask, decoded.bitfield_tmask);
                     printf("\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\n",
                             decoded.extend_type, decoded.shift_type,
                         decoded.shift_amount, decoded.immr, decoded.imms,
                 decoded.simd_cmode, decoded.condition, decoded.nzcv, decoded.sysreg,
                 decoded.element_width, decoded.lane_index,
             decoded.rd, decoded.rn, decoded.rm, decoded.ra,
             decoded.rt, decoded.rt2, decoded.rs, decoded.operand_width);
        index++;
    }

    if (ferror(input))
    {
        perror(argv[1]);
        failures++;
    }
    fclose(input);
    fprintf(stderr, "ARM64 instruction decoder: rows=%zu failures=%u\n", index, failures);
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}