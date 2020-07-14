#include <string>
#include <cstddef>
#include <unordered_map>
#include <memory>

#include <wiz/ast/expression.h>
#include <wiz/ast/type_expression.h>
#include <wiz/ast/statement.h>
#include <wiz/compiler/bank.h>
#include <wiz/compiler/builtins.h>
#include <wiz/compiler/compiler.h>
#include <wiz/compiler/definition.h>
#include <wiz/compiler/instruction.h>
#include <wiz/compiler/symbol_table.h>
#include <wiz/utility/report.h>
#include <wiz/utility/misc.h>
#include <wiz/platform/z80_platform.h>

namespace wiz {
    Z80Platform::Z80Platform() {}
    Z80Platform::~Z80Platform() {}

    void Z80Platform::reserveDefinitions(Builtins& builtins) {
        // http://clrhome.org/table/

        builtins.addDefineBoolean("__family_z80"_sv, true);
        builtins.addDefineBoolean("__cpu_z80"_sv, true);
        // TODO: other Z80 revisions, like Z180, eZ80.

        auto stringPool = builtins.getStringPool();
        auto scope = builtins.getBuiltinScope();
        const auto decl = builtins.getBuiltinDeclaration();
        const auto u8Type = builtins.getDefinition(Builtins::DefinitionType::U8);
        const auto u16Type = builtins.getDefinition(Builtins::DefinitionType::U16);
        const auto u24Type = builtins.getDefinition(Builtins::DefinitionType::U24);
        const auto boolType = builtins.getDefinition(Builtins::DefinitionType::Bool);

        bitIndex7Expression = makeFwdUnique<Expression>(
            Expression::IntegerLiteral(Int128(7)),
            decl->location, 
            ExpressionInfo(EvaluationContext::CompileTime, 
                makeFwdUnique<TypeExpression>(TypeExpression::ResolvedIdentifier(u8Type), decl->location),
                Qualifiers {}));
        
        pointerSizedType = u16Type;
        farPointerSizedType = u24Type;

        // Registers.
        a = scope->createDefinition(nullptr, Definition::BuiltinRegister(u8Type), stringPool->intern("a"), decl);
        b = scope->createDefinition(nullptr, Definition::BuiltinRegister(u8Type), stringPool->intern("b"), decl);
        const auto c = scope->createDefinition(nullptr, Definition::BuiltinRegister(u8Type), stringPool->intern("c"), decl);
        const auto d = scope->createDefinition(nullptr, Definition::BuiltinRegister(u8Type), stringPool->intern("d"), decl);
        const auto e = scope->createDefinition(nullptr, Definition::BuiltinRegister(u8Type), stringPool->intern("e"), decl);
        const auto h = scope->createDefinition(nullptr, Definition::BuiltinRegister(u8Type), stringPool->intern("h"), decl);
        const auto l = scope->createDefinition(nullptr, Definition::BuiltinRegister(u8Type), stringPool->intern("l"), decl);
        const auto ixh = scope->createDefinition(nullptr, Definition::BuiltinRegister(u8Type), stringPool->intern("ixh"), decl);
        const auto ixl = scope->createDefinition(nullptr, Definition::BuiltinRegister(u8Type), stringPool->intern("ixl"), decl);
        const auto iyh = scope->createDefinition(nullptr, Definition::BuiltinRegister(u8Type), stringPool->intern("iyh"), decl);
        const auto iyl = scope->createDefinition(nullptr, Definition::BuiltinRegister(u8Type), stringPool->intern("iyl"), decl);
        const auto af = scope->createDefinition(nullptr, Definition::BuiltinRegister(u16Type), stringPool->intern("af"), decl);
        const auto bc = scope->createDefinition(nullptr, Definition::BuiltinRegister(u16Type), stringPool->intern("bc"), decl);
        const auto de = scope->createDefinition(nullptr, Definition::BuiltinRegister(u16Type), stringPool->intern("de"), decl);
        const auto hl = scope->createDefinition(nullptr, Definition::BuiltinRegister(u16Type), stringPool->intern("hl"), decl);
        const auto sp = scope->createDefinition(nullptr, Definition::BuiltinRegister(u16Type), stringPool->intern("sp"), decl);
        const auto ix = scope->createDefinition(nullptr, Definition::BuiltinRegister(u16Type), stringPool->intern("ix"), decl);
        const auto iy = scope->createDefinition(nullptr, Definition::BuiltinRegister(u16Type), stringPool->intern("iy"), decl);

        builtins.addRegisterDecomposition(bc, {c, b});
        builtins.addRegisterDecomposition(de, {e, d});
        builtins.addRegisterDecomposition(hl, {l, h});
        builtins.addRegisterDecomposition(ix, {ixl, ixh});
        builtins.addRegisterDecomposition(iy, {iyl, iyh});

        const auto patternA = builtins.createInstructionOperandPattern(InstructionOperandPattern::Register(a));
        const auto patternB = builtins.createInstructionOperandPattern(InstructionOperandPattern::Register(b));
        const auto patternC = builtins.createInstructionOperandPattern(InstructionOperandPattern::Register(c));
        const auto patternD = builtins.createInstructionOperandPattern(InstructionOperandPattern::Register(d));
        const auto patternE = builtins.createInstructionOperandPattern(InstructionOperandPattern::Register(e));
        const auto patternH = builtins.createInstructionOperandPattern(InstructionOperandPattern::Register(h));
        const auto patternL = builtins.createInstructionOperandPattern(InstructionOperandPattern::Register(l));
        const auto patternIXH = builtins.createInstructionOperandPattern(InstructionOperandPattern::Register(ixh));
        const auto patternIXL = builtins.createInstructionOperandPattern(InstructionOperandPattern::Register(ixl));
        const auto patternIYH = builtins.createInstructionOperandPattern(InstructionOperandPattern::Register(iyh));
        const auto patternIYL = builtins.createInstructionOperandPattern(InstructionOperandPattern::Register(iyl));
        const auto patternAF = builtins.createInstructionOperandPattern(InstructionOperandPattern::Register(af));
        const auto patternBC = builtins.createInstructionOperandPattern(InstructionOperandPattern::Register(bc));
        const auto patternDE = builtins.createInstructionOperandPattern(InstructionOperandPattern::Register(de));
        const auto patternHL = builtins.createInstructionOperandPattern(InstructionOperandPattern::Register(hl));
        const auto patternSP = builtins.createInstructionOperandPattern(InstructionOperandPattern::Register(sp));
        const auto patternIX = builtins.createInstructionOperandPattern(InstructionOperandPattern::Register(ix));
        const auto patternIY = builtins.createInstructionOperandPattern(InstructionOperandPattern::Register(iy));

        carry = scope->createDefinition(nullptr, Definition::BuiltinRegister(boolType), stringPool->intern("carry"), decl);
        zero = scope->createDefinition(nullptr, Definition::BuiltinRegister(boolType), stringPool->intern("zero"), decl);
        negative = scope->createDefinition(nullptr, Definition::BuiltinRegister(boolType), stringPool->intern("negative"), decl);
        const auto patternZero = builtins.createInstructionOperandPattern(InstructionOperandPattern::Register(zero));
        const auto patternCarry = builtins.createInstructionOperandPattern(InstructionOperandPattern::Register(carry));
        const auto patternOverflow = builtins.createInstructionOperandPattern(InstructionOperandPattern::Register(scope->createDefinition(nullptr, Definition::BuiltinRegister(boolType), stringPool->intern("overflow"), decl)));
        const auto patternNegative = builtins.createInstructionOperandPattern(InstructionOperandPattern::Register(negative));
        const auto patternInterrupt = builtins.createInstructionOperandPattern(InstructionOperandPattern::Register(scope->createDefinition(nullptr, Definition::BuiltinRegister(boolType), stringPool->intern("interrupt"), decl)));
        const auto patternInterruptMode = builtins.createInstructionOperandPattern(InstructionOperandPattern::Register(scope->createDefinition(nullptr, Definition::BuiltinRegister(u8Type), stringPool->intern("interrupt_mode"), decl)));
        const auto patternInterruptPage = builtins.createInstructionOperandPattern(InstructionOperandPattern::Register(scope->createDefinition(nullptr, Definition::BuiltinRegister(u8Type), stringPool->intern("interrupt_page"), decl)));
        const auto patternMemoryRefresh = builtins.createInstructionOperandPattern(InstructionOperandPattern::Register(scope->createDefinition(nullptr, Definition::BuiltinRegister(u8Type), stringPool->intern("memory_refresh"), decl)));

        // Intrinsics.
        const auto push = scope->createDefinition(nullptr, Definition::BuiltinVoidIntrinsic(), stringPool->intern("push"), decl);
        const auto pop = scope->createDefinition(nullptr, Definition::BuiltinLoadIntrinsic(u16Type), stringPool->intern("pop"), decl);
        const auto nop = scope->createDefinition(nullptr, Definition::BuiltinVoidIntrinsic(), stringPool->intern("nop"), decl);
        const auto halt = scope->createDefinition(nullptr, Definition::BuiltinVoidIntrinsic(), stringPool->intern("halt"), decl);
        const auto decimal_adjust = scope->createDefinition(nullptr, Definition::BuiltinVoidIntrinsic(), stringPool->intern("decimal_adjust"), decl);
        const auto exchange_16_bit_registers = scope->createDefinition(nullptr, Definition::BuiltinVoidIntrinsic(), stringPool->intern("exchange_16_bit_registers"), decl);
        const auto swap = scope->createDefinition(nullptr, Definition::BuiltinVoidIntrinsic(), stringPool->intern("swap"), decl);
        const auto swap_shadow = scope->createDefinition(nullptr, Definition::BuiltinVoidIntrinsic(), stringPool->intern("swap_shadow"), decl);
        const auto load_inc_next = scope->createDefinition(nullptr, Definition::BuiltinVoidIntrinsic(), stringPool->intern("load_inc_next"), decl);
        const auto load_dec_next = scope->createDefinition(nullptr, Definition::BuiltinVoidIntrinsic(), stringPool->intern("load_dec_next"), decl);
        const auto load_inc_repeat = scope->createDefinition(nullptr, Definition::BuiltinVoidIntrinsic(), stringPool->intern("load_inc_repeat"), decl);
        const auto load_dec_repeat = scope->createDefinition(nullptr, Definition::BuiltinVoidIntrinsic(), stringPool->intern("load_dec_repeat"), decl);
        bit = scope->createDefinition(nullptr, Definition::BuiltinVoidIntrinsic(), stringPool->intern("bit"), decl);
        cmp = scope->createDefinition(nullptr, Definition::BuiltinVoidIntrinsic(), stringPool->intern("cmp"), decl);
        const auto cmp_inc_next = scope->createDefinition(nullptr, Definition::BuiltinVoidIntrinsic(), stringPool->intern("cmp_inc_next"), decl);
        const auto cmp_dec_next = scope->createDefinition(nullptr, Definition::BuiltinVoidIntrinsic(), stringPool->intern("cmp_dec_next"), decl);
        const auto cmp_inc_repeat = scope->createDefinition(nullptr, Definition::BuiltinVoidIntrinsic(), stringPool->intern("cmp_inc_repeat"), decl);
        const auto cmp_dec_repeat = scope->createDefinition(nullptr, Definition::BuiltinVoidIntrinsic(), stringPool->intern("cmp_dec_repeat"), decl);
        const auto rotate_left_digits = scope->createDefinition(nullptr, Definition::BuiltinVoidIntrinsic(), stringPool->intern("rotate_left_digits"), decl);
        const auto rotate_right_digits = scope->createDefinition(nullptr, Definition::BuiltinVoidIntrinsic(), stringPool->intern("rotate_right_digits"), decl);
        const auto io_read = scope->createDefinition(nullptr, Definition::BuiltinLoadIntrinsic(u8Type), stringPool->intern("io_read"), decl);
        const auto io_read_inc_next = scope->createDefinition(nullptr, Definition::BuiltinVoidIntrinsic(), stringPool->intern("io_read_inc_next"), decl);
        const auto io_read_dec_next = scope->createDefinition(nullptr, Definition::BuiltinVoidIntrinsic(), stringPool->intern("io_read_dec_next"), decl);
        const auto io_read_inc_repeat = scope->createDefinition(nullptr, Definition::BuiltinVoidIntrinsic(), stringPool->intern("io_read_inc_repeat"), decl);
        const auto io_read_dec_repeat = scope->createDefinition(nullptr, Definition::BuiltinVoidIntrinsic(), stringPool->intern("io_read_dec_repeat"), decl);
        const auto io_write = scope->createDefinition(nullptr, Definition::BuiltinVoidIntrinsic(), stringPool->intern("io_write"), decl);
        const auto io_write_inc_next = scope->createDefinition(nullptr, Definition::BuiltinVoidIntrinsic(), stringPool->intern("io_write_inc_next"), decl);
        const auto io_write_dec_next = scope->createDefinition(nullptr, Definition::BuiltinVoidIntrinsic(), stringPool->intern("io_write_dec_next"), decl);
        const auto io_write_inc_repeat = scope->createDefinition(nullptr, Definition::BuiltinVoidIntrinsic(), stringPool->intern("io_write_inc_repeat"), decl);
        const auto io_write_dec_repeat = scope->createDefinition(nullptr, Definition::BuiltinVoidIntrinsic(), stringPool->intern("io_write_dec_repeat"), decl);
		dec_branch_not_zero = scope->createDefinition(nullptr, Definition::BuiltinVoidIntrinsic(), stringPool->intern("dec_branch_not_zero"), decl);

        // Non-register operands.
        const auto patternFalse = builtins.createInstructionOperandPattern(InstructionOperandPattern::Boolean(false));
        const auto patternTrue = builtins.createInstructionOperandPattern(InstructionOperandPattern::Boolean(true));
        const auto patternAtLeast0 = builtins.createInstructionOperandPattern(InstructionOperandPattern::IntegerAtLeast(Int128(0)));
        const auto patternAtLeast1 = builtins.createInstructionOperandPattern(InstructionOperandPattern::IntegerAtLeast(Int128(1)));
        const auto pattern0 = builtins.createInstructionOperandPattern(InstructionOperandPattern::IntegerRange(Int128(0)));
        const auto pattern1 = builtins.createInstructionOperandPattern(InstructionOperandPattern::IntegerRange(Int128(1)));
        const auto pattern2 = builtins.createInstructionOperandPattern(InstructionOperandPattern::IntegerRange(Int128(2)));
        const auto pattern8 = builtins.createInstructionOperandPattern(InstructionOperandPattern::IntegerRange(Int128(8)));
        const auto pattern16 = builtins.createInstructionOperandPattern(InstructionOperandPattern::IntegerRange(Int128(16)));
        const auto pattern24 = builtins.createInstructionOperandPattern(InstructionOperandPattern::IntegerRange(Int128(24)));
        const auto pattern32 = builtins.createInstructionOperandPattern(InstructionOperandPattern::IntegerRange(Int128(32)));
        const auto pattern40 = builtins.createInstructionOperandPattern(InstructionOperandPattern::IntegerRange(Int128(40)));
        const auto pattern48 = builtins.createInstructionOperandPattern(InstructionOperandPattern::IntegerRange(Int128(48)));
        const auto pattern56 = builtins.createInstructionOperandPattern(InstructionOperandPattern::IntegerRange(Int128(56)));
        const auto patternIndirectBC = builtins.createInstructionOperandPattern(InstructionOperandPattern::Dereference(false, patternBC->clone(), 1));
        const auto patternIndirectDE = builtins.createInstructionOperandPattern(InstructionOperandPattern::Dereference(false, patternDE->clone(), 1));
        const auto patternIndirectHL = builtins.createInstructionOperandPattern(InstructionOperandPattern::Dereference(false, patternHL->clone(), 1));
        const auto patternIndirectSP = builtins.createInstructionOperandPattern(InstructionOperandPattern::Dereference(false, patternSP->clone(), 1));
        const auto patternImmBitSubscript = builtins.createInstructionOperandPattern(InstructionOperandPattern::IntegerRange(Int128(0), Int128(7)));
        const auto patternImmU8 = builtins.createInstructionOperandPattern(InstructionOperandPattern::IntegerRange(Int128(0), Int128(0xFF)));
        const auto patternImmU16 = builtins.createInstructionOperandPattern(InstructionOperandPattern::IntegerRange(Int128(0), Int128(0xFFFF)));
        const auto patternImmI8 = builtins.createInstructionOperandPattern(InstructionOperandPattern::IntegerRange(Int128(-0x80), Int128(0x7F)));
        const auto patternIndexIX
            = builtins.createInstructionOperandPattern(InstructionOperandPattern::Index(
                false,
                patternIX->clone(),
                makeFwdUnique<InstructionOperandPattern>(InstructionOperandPattern::Capture(
                    patternImmI8->clone())),
                1, 1));
        const auto patternIndexIY
            = builtins.createInstructionOperandPattern(InstructionOperandPattern::Index(
                false,
                patternIY->clone(),
                makeFwdUnique<InstructionOperandPattern>(InstructionOperandPattern::Capture(
                    patternImmI8->clone())),
                1, 1));
        const auto patternAbsU8
            = builtins.createInstructionOperandPattern(InstructionOperandPattern::Dereference(
                false,
                makeFwdUnique<InstructionOperandPattern>(InstructionOperandPattern::Capture(
                    patternImmU16->clone())),
                1));
        const auto patternAbsU16
            = builtins.createInstructionOperandPattern(InstructionOperandPattern::Dereference(
                false,
                makeFwdUnique<InstructionOperandPattern>(InstructionOperandPattern::Capture(
                    patternImmU16->clone())),
                2));

        // Instruction encodings.
        const auto encodingImplicit = builtins.createInstructionEncoding(
            [](const InstructionOptions& options, const std::vector<std::vector<const InstructionOperand*>>& captureLists) {
                static_cast<void>(captureLists);
                return options.opcode.size();
            },
            [](Report* report, const Bank* bank, std::vector<std::uint8_t>& buffer, const InstructionOptions& options, const std::vector<std::vector<const InstructionOperand*>>& captureLists, SourceLocation location) {
                static_cast<void>(report);
                static_cast<void>(bank);
                static_cast<void>(captureLists);
                static_cast<void>(location);

                buffer.insert(buffer.end(), options.opcode.begin(), options.opcode.end());
                return true;
            });
        const auto encodingU8Operand = builtins.createInstructionEncoding(
            [](const InstructionOptions& options, const std::vector<std::vector<const InstructionOperand*>>& captureLists) {
                static_cast<void>(captureLists);
                return options.opcode.size() + 1;
            },
            [](Report* report, const Bank* bank, std::vector<std::uint8_t>& buffer, const InstructionOptions& options, const std::vector<std::vector<const InstructionOperand*>>& captureLists, SourceLocation location) {
                static_cast<void>(report);
                static_cast<void>(bank);
                static_cast<void>(location);

                buffer.insert(buffer.end(), options.opcode.begin(), options.opcode.end());
                buffer.push_back(static_cast<std::uint8_t>(captureLists[options.parameter[0]][0]->variant.get<InstructionOperand::Integer>().value));
                return true;
            });
        const auto encodingU16Operand = builtins.createInstructionEncoding(
            [](const InstructionOptions& options, const std::vector<std::vector<const InstructionOperand*>>& captureLists) {
                static_cast<void>(captureLists);
                return options.opcode.size() + 2;
            },
            [](Report* report, const Bank* bank, std::vector<std::uint8_t>& buffer, const InstructionOptions& options, const std::vector<std::vector<const InstructionOperand*>>& captureLists, SourceLocation location) {
                static_cast<void>(report);
                static_cast<void>(bank);
                static_cast<void>(location);

                buffer.insert(buffer.end(), options.opcode.begin(), options.opcode.end());
                const auto value = static_cast<std::uint16_t>(captureLists[options.parameter[0]][0]->variant.get<InstructionOperand::Integer>().value);
                buffer.push_back(static_cast<std::uint8_t>(value & 0xFF));
                buffer.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
                return true;
            });
        const auto encodingPCRelativeI8Operand = builtins.createInstructionEncoding(
            [](const InstructionOptions& options, const std::vector<std::vector<const InstructionOperand*>>& captureLists) {
                static_cast<void>(captureLists);
                return options.opcode.size() + 1;
            },
            [](Report* report, const Bank* bank, std::vector<std::uint8_t>& buffer, const InstructionOptions& options, const std::vector<std::vector<const InstructionOperand*>>& captureLists, SourceLocation location) {
                buffer.insert(buffer.end(), options.opcode.begin(), options.opcode.end());

                const auto base = static_cast<std::int32_t>(bank->getAddress().absolutePosition.get() & 0xFFFF);
                const auto dest = static_cast<std::int32_t>(captureLists[options.parameter[0]][0]->variant.get<InstructionOperand::Integer>().value);
                const auto offset = dest - base - 2;
                if (offset >= -128 && offset <= 127) {
                    buffer.push_back(offset < 0
                        ? (static_cast<std::uint8_t>(-offset) ^ 0xFF) + 1
                        : static_cast<std::uint8_t>(offset));
                } else {
                    buffer.push_back(0);
                    report->error("pc-relative offset is outside of representable signed 8-bit range -128..127", location);
                    return false;
                }
                return true;
            });
        const auto encodingI8Operand = builtins.createInstructionEncoding(
            [](const InstructionOptions& options, const std::vector<std::vector<const InstructionOperand*>>& captureLists) {
                static_cast<void>(captureLists);
                return options.opcode.size() + 1;
            },
            [](Report* report, const Bank* bank, std::vector<std::uint8_t>& buffer, const InstructionOptions& options, const std::vector<std::vector<const InstructionOperand*>>& captureLists, SourceLocation location) {
                static_cast<void>(bank);

                buffer.insert(buffer.end(), options.opcode.begin(), options.opcode.end());

                const auto i8val = static_cast<int>(captureLists[options.parameter[0]][0]->variant.get<InstructionOperand::Integer>().value);
                if (i8val >= -128 && i8val <= 127) {
                    buffer.push_back(i8val < 0
                        ? (static_cast<std::uint8_t>(-i8val) ^ 0xFF) + 1
                        : static_cast<std::uint8_t>(i8val));
                } else {
                    buffer.push_back(0);
                    report->error("signed value is outside of representable signed 8-bit range -128..127", location);
                    return false;
                }
                return true;
            });
        const auto encodingI8OperandU8Operand = builtins.createInstructionEncoding(
            [](const InstructionOptions& options, const std::vector<std::vector<const InstructionOperand*>>& captureLists) {
                static_cast<void>(captureLists);
                return options.opcode.size() + 2;
            },
            [](Report* report, const Bank* bank, std::vector<std::uint8_t>& buffer, const InstructionOptions& options, const std::vector<std::vector<const InstructionOperand*>>& captureLists, SourceLocation location) {
                static_cast<void>(bank);

                buffer.insert(buffer.end(), options.opcode.begin(), options.opcode.end());

                const auto i8val = static_cast<int>(captureLists[options.parameter[0]][0]->variant.get<InstructionOperand::Integer>().value);
                if (i8val >= -128 && i8val <= 127) {
                    buffer.push_back(i8val < 0
                        ? (static_cast<std::uint8_t>(-i8val) ^ 0xFF) + 1
                        : static_cast<std::uint8_t>(i8val));
                } else {
                    buffer.push_back(0);
                    buffer.push_back(0);
                    report->error("signed value is outside of representable signed 8-bit range -128..127", location);
                    return false;
                }

                buffer.push_back(static_cast<std::uint8_t>(captureLists[options.parameter[1]][0]->variant.get<InstructionOperand::Integer>().value));
                return true;
            });
        const auto encodingRepeatedImplicit = builtins.createInstructionEncoding(
            [](const InstructionOptions& options, const std::vector<std::vector<const InstructionOperand*>>& captureLists) {
                return static_cast<std::size_t>(captureLists[options.parameter[0]][0]->variant.get<InstructionOperand::Integer>().value) * options.opcode.size();
            },
            [](Report* report, const Bank* bank, std::vector<std::uint8_t>& buffer, const InstructionOptions& options, const std::vector<std::vector<const InstructionOperand*>>& captureLists, SourceLocation location) {
                static_cast<void>(report);
                static_cast<void>(bank);
                static_cast<void>(location);

                const auto count = static_cast<std::uint8_t>(captureLists[options.parameter[0]][0]->variant.get<InstructionOperand::Integer>().value);
                for (std::size_t i = 0; i != count; ++i) {
                    buffer.insert(buffer.end(), options.opcode.begin(), options.opcode.end());
                }
                return true;
            });
        const auto encodingRepeatedI8Operand = builtins.createInstructionEncoding(
            [](const InstructionOptions& options, const std::vector<std::vector<const InstructionOperand*>>& captureLists) {
                return static_cast<std::size_t>(captureLists[options.parameter[1]][0]->variant.get<InstructionOperand::Integer>().value) * (options.opcode.size() + 1);
            },
            [](Report* report, const Bank* bank, std::vector<std::uint8_t>& buffer, const InstructionOptions& options, const std::vector<std::vector<const InstructionOperand*>>& captureLists, SourceLocation location) {
                static_cast<void>(bank);

                const auto i8val = static_cast<int>(captureLists[options.parameter[0]][0]->variant.get<InstructionOperand::Integer>().value);

                std::uint8_t byteVal = 0;
                if (i8val < -128 || i8val > 127) {
                    byteVal = i8val < 0
                        ? (static_cast<std::uint8_t>(-i8val) ^ 0xFF) + 1
                        : static_cast<std::uint8_t>(i8val);
                } else {                    
                    report->error("signed value is outside of representable signed 8-bit range -128..127", location);
                }

                const auto count = static_cast<std::uint8_t>(captureLists[options.parameter[1]][0]->variant.get<InstructionOperand::Integer>().value);
                for (std::size_t i = 0; i != count; ++i) {
                    buffer.insert(buffer.end(), options.opcode.begin(), options.opcode.end());
                    buffer.push_back(byteVal);
                }
                return true;
            });
        const auto encodingBitIndex = builtins.createInstructionEncoding(
            [](const InstructionOptions& options, const std::vector<std::vector<const InstructionOperand*>>& captureLists) {
                static_cast<void>(captureLists);
                return options.opcode.size();
            },
            [](Report* report, const Bank* bank, std::vector<std::uint8_t>& buffer, const InstructionOptions& options, const std::vector<std::vector<const InstructionOperand*>>& captureLists, SourceLocation location) {
                static_cast<void>(report);
                static_cast<void>(bank);
                static_cast<void>(location);

                // n = $1th capture of $0
                buffer.insert(buffer.end(), options.opcode.begin(), options.opcode.end());
                const auto n = static_cast<std::uint8_t>(captureLists[options.parameter[0]][options.parameter[1]]->variant.get<InstructionOperand::Integer>().value);
                buffer[buffer.size() - 1] |= static_cast<std::uint8_t>(n << 3);
                return true;
            });
        const auto encodingBitIndexI8Operand = builtins.createInstructionEncoding(
            [](const InstructionOptions& options, const std::vector<std::vector<const InstructionOperand*>>& captureLists) {
                static_cast<void>(captureLists);
                return options.opcode.size() + 1;
            },
            [](Report* report, const Bank* bank, std::vector<std::uint8_t>& buffer, const InstructionOptions& options, const std::vector<std::vector<const InstructionOperand*>>& captureLists, SourceLocation location) {
                static_cast<void>(bank);

                // *(ix + dd) $ n bit-wise access.
                // dd = 0th capture of $0
                // n = $2th capture of $1
                buffer.insert(buffer.end(), options.opcode.begin(), options.opcode.end());
                const auto n = static_cast<std::uint8_t>(captureLists[options.parameter[1]][options.parameter[2]]->variant.get<InstructionOperand::Integer>().value);
                buffer[buffer.size() - 1] |= static_cast<std::uint8_t>(n << 3);

                const auto i8val = static_cast<int>(captureLists[options.parameter[0]][0]->variant.get<InstructionOperand::Integer>().value);
                if (i8val >= -128 && i8val <= 127) {
                    buffer.push_back(i8val < 0
                        ? (static_cast<std::uint8_t>(-i8val) ^ 0xFF) + 1
                        : static_cast<std::uint8_t>(i8val));
                    return true;
                } else {
                    buffer.push_back(0);
                    report->error("signed value is outside of representable signed 8-bit range -128..127", location);
                    return false;
                }
            });

        // Prefixes.
        const std::uint8_t prefixIX = 0xDD;
        const std::uint8_t prefixIY = 0xFD;
        const std::uint8_t prefixExtended = 0xED;
        const std::uint8_t prefixBit = 0xCB;

        // Register info.
        using RegisterInfo = std::tuple<const InstructionOperandPattern*, std::uint8_t, std::size_t>;
        const RegisterInfo generalRegisters[] {
            RegisterInfo {patternB, 0, SIZE_MAX},
            RegisterInfo {patternC, 1, SIZE_MAX},
            RegisterInfo {patternD, 2, SIZE_MAX},
            RegisterInfo {patternE, 3, SIZE_MAX},
            RegisterInfo {patternH, 4, 0},
            RegisterInfo {patternL, 5, 1},
            RegisterInfo {patternIndirectHL, 6, 3},
            RegisterInfo {patternA, 7, SIZE_MAX},
        };
        const RegisterInfo generalRegisterPairs[] {
            RegisterInfo {patternBC, 0, SIZE_MAX},
            RegisterInfo {patternDE, 1, SIZE_MAX},
            RegisterInfo {patternHL, 2, 2},
            RegisterInfo {patternSP, 3, SIZE_MAX},
        };
        using PrefixSet = std::tuple<std::uint8_t, std::vector<const InstructionOperandPattern*>>;
        const PrefixSet prefixSets[] {
            PrefixSet {prefixIX, {patternIXH, patternIXL, patternIX, patternIndexIX}},
            PrefixSet {prefixIY, {patternIYH, patternIYL, patternIY, patternIndexIY}},
        };

        // Instructions.
        // r = r2
        for (const auto& destRegister : generalRegisters) {
            for (const auto& sourceRegister : generalRegisters) {
                if (std::get<0>(destRegister) == std::get<0>(sourceRegister)) {
                    continue;
                }

                std::vector<std::uint8_t> opcode {static_cast<std::uint8_t>(0x40 | (std::get<1>(destRegister) << 3) | std::get<1>(sourceRegister))};
                const auto destRegisterOperand = std::get<0>(destRegister);
                const auto sourceRegisterOperand = std::get<0>(sourceRegister);
                const auto prefixedDestRegisterIndex = std::get<2>(destRegister);
                const auto prefixedSourceRegisterIndex = std::get<2>(sourceRegister);

                builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Assignment), 0, {destRegisterOperand, sourceRegisterOperand}), encodingImplicit, InstructionOptions(opcode, {}, {}));

                if (prefixedDestRegisterIndex != SIZE_MAX
                || prefixedSourceRegisterIndex != SIZE_MAX) {
                    for (const auto& prefixSet : prefixSets) {
                        std::vector<std::uint8_t> prefixedOpcode {std::get<0>(prefixSet)};
                        prefixedOpcode.insert(prefixedOpcode.end(), opcode.begin(), opcode.end());

                        const auto prefixedDest = prefixedDestRegisterIndex != SIZE_MAX ? std::get<1>(prefixSet)[prefixedDestRegisterIndex] : destRegisterOperand;
                        const auto prefixedSource = prefixedSourceRegisterIndex != SIZE_MAX ? std::get<1>(prefixSet)[prefixedSourceRegisterIndex] : sourceRegisterOperand;

                        std::vector<std::size_t> prefixedInstructionOptions;
                        auto prefixedEncoding = encodingImplicit;
                        if (prefixedDestRegisterIndex != SIZE_MAX && prefixedDest->variant.is<InstructionOperandPattern::Index>()) {
                            prefixedInstructionOptions.push_back(0);
                            prefixedEncoding = encodingI8Operand;
                        } else if (prefixedSourceRegisterIndex != SIZE_MAX && prefixedSource->variant.is<InstructionOperandPattern::Index>()) {
                            prefixedInstructionOptions.push_back(1);
                            prefixedEncoding = encodingI8Operand;
                        }

                        builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Assignment), 0, {prefixedDest, prefixedSource}), prefixedEncoding, InstructionOptions(prefixedOpcode, prefixedInstructionOptions, {}));
                    }
                }
            }
        }        
        // r = n
        for (const auto& destRegister : generalRegisters) {
            std::vector<std::uint8_t> opcode {static_cast<std::uint8_t>(0x6 | (std::get<1>(destRegister) << 3))};

            const auto destRegisterOperand = std::get<0>(destRegister);
            const auto prefixedDestRegisterIndex = std::get<2>(destRegister);

            builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Assignment), 0, {destRegisterOperand, patternImmU8}), encodingU8Operand, InstructionOptions(opcode, {1}, {}));

            if (prefixedDestRegisterIndex != SIZE_MAX) {
                for (const auto& prefixSet : prefixSets) {
                    std::vector<std::uint8_t> prefixedOpcode {std::get<0>(prefixSet)};
                    prefixedOpcode.insert(prefixedOpcode.end(), opcode.begin(), opcode.end());

                    const auto prefixedDest = std::get<1>(prefixSet)[prefixedDestRegisterIndex];

                    std::vector<std::size_t> prefixedInstructionOptions;
                    auto prefixedEncoding = encodingU8Operand;
                    if (prefixedDest->variant.is<InstructionOperandPattern::Index>()) {
                        prefixedInstructionOptions.push_back(0);
                        prefixedEncoding = encodingI8OperandU8Operand;
                    }
                    prefixedInstructionOptions.push_back(1);

                    builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Assignment), 0, {prefixedDest, patternImmU8}), prefixedEncoding, InstructionOptions(prefixedOpcode, prefixedInstructionOptions, {}));
                }
            }
        }
        // a = 0 (a = a ^ a)
        // FIXME: xor clears the carry. sometimes, we don't want to clear the carry when loading zero (eg. ld 0 followed by adc). need a way to disambiguate the instructions. for now disallow this optimization.
        //builtins.emplaceInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Assignment), 0, {patternA, pattern0}), encodingImplicit, InstructionOptions({0xAF}, {}, {}));

        // a = *(bc)
        // *(bc) = a
        builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Assignment), 0, {patternA, patternIndirectBC}), encodingImplicit, InstructionOptions({0x0A}, {}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Assignment), 0, {patternIndirectBC, patternA}), encodingImplicit, InstructionOptions({0x02}, {}, {}));
        // a = *(de)
        // *(de) = a
        builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Assignment), 0, {patternA, patternIndirectDE}), encodingImplicit, InstructionOptions({0x1A}, {}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Assignment), 0, {patternIndirectDE, patternA}), encodingImplicit, InstructionOptions({0x12}, {}, {}));
        // a = *(nn)
        // *(nn) = a
        builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Assignment), 0, {patternA, patternAbsU8}), encodingU16Operand, InstructionOptions({0x3A}, {1}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Assignment), 0, {patternAbsU8, patternA}), encodingU16Operand, InstructionOptions({0x32}, {0}, {}));
        // a = interrupt_page
        // interrupt_page = a
        builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Assignment), 0, {patternA, patternInterruptPage}), encodingImplicit, InstructionOptions({prefixExtended, 0x57}, {}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Assignment), 0, {patternInterruptPage, patternA}), encodingImplicit, InstructionOptions({prefixExtended, 0x47}, {}, {}));
        // a = memory_refresh
        // memory_refresh = a
        builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Assignment), 0, {patternA, patternMemoryRefresh}), encodingImplicit, InstructionOptions({prefixExtended, 0x5F}, {}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Assignment), 0, {patternMemoryRefresh, patternA}), encodingImplicit, InstructionOptions({prefixExtended, 0x4F}, {}, {}));
        // rr = nn
        builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Assignment), 0, {patternBC, patternImmU16}), encodingU16Operand, InstructionOptions({0x01}, {1}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Assignment), 0, {patternDE, patternImmU16}), encodingU16Operand, InstructionOptions({0x11}, {1}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Assignment), 0, {patternHL, patternImmU16}), encodingU16Operand, InstructionOptions({0x21}, {1}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Assignment), 0, {patternSP, patternImmU16}), encodingU16Operand, InstructionOptions({0x31}, {1}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Assignment), 0, {patternIX, patternImmU16}), encodingU16Operand, InstructionOptions({prefixIX, 0x21}, {1}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Assignment), 0, {patternIY, patternImmU16}), encodingU16Operand, InstructionOptions({prefixIY, 0x21}, {1}, {}));
        // rr = *(nn)
        builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Assignment), 0, {patternBC, patternAbsU16}), encodingU16Operand, InstructionOptions({prefixExtended, 0x4B}, {1}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Assignment), 0, {patternDE, patternAbsU16}), encodingU16Operand, InstructionOptions({prefixExtended, 0x5B}, {1}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Assignment), 0, {patternHL, patternAbsU16}), encodingU16Operand, InstructionOptions({0x2A}, {1}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Assignment), 0, {patternSP, patternAbsU16}), encodingU16Operand, InstructionOptions({prefixExtended, 0x7B}, {1}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Assignment), 0, {patternIX, patternAbsU16}), encodingU16Operand, InstructionOptions({prefixIX, 0x2A}, {1}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Assignment), 0, {patternIY, patternAbsU16}), encodingU16Operand, InstructionOptions({prefixIY, 0x2A}, {1}, {}));
        // *(nn) = rr
        builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Assignment), 0, {patternAbsU16, patternBC}), encodingU16Operand, InstructionOptions({prefixExtended, 0x43}, {0}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Assignment), 0, {patternAbsU16, patternDE}), encodingU16Operand, InstructionOptions({prefixExtended, 0x53}, {0}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Assignment), 0, {patternAbsU16, patternHL}), encodingU16Operand, InstructionOptions({0x22}, {0}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Assignment), 0, {patternAbsU16, patternSP}), encodingU16Operand, InstructionOptions({prefixExtended, 0x73}, {0}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Assignment), 0, {patternAbsU16, patternIX}), encodingU16Operand, InstructionOptions({prefixIX, 0x22}, {0}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Assignment), 0, {patternAbsU16, patternIY}), encodingU16Operand, InstructionOptions({prefixIY, 0x22}, {0}, {}));
        // sp = rr
        builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Assignment), 0, {patternSP, patternHL}), encodingImplicit, InstructionOptions({0xF9}, {}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Assignment), 0, {patternSP, patternIX}), encodingImplicit, InstructionOptions({prefixIX, 0xF9}, {}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Assignment), 0, {patternSP, patternIY}), encodingImplicit, InstructionOptions({prefixIY, 0xF9}, {}, {}));
        // push(rr)
        builtins.createInstruction(InstructionSignature(InstructionType(InstructionType::VoidIntrinsic(push)), 0, {patternBC}), encodingImplicit, InstructionOptions({0xC5}, {}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(InstructionType::VoidIntrinsic(push)), 0, {patternDE}), encodingImplicit, InstructionOptions({0xD5}, {}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(InstructionType::VoidIntrinsic(push)), 0, {patternHL}), encodingImplicit, InstructionOptions({0xE5}, {}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(InstructionType::VoidIntrinsic(push)), 0, {patternAF}), encodingImplicit, InstructionOptions({0xF5}, {}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(InstructionType::VoidIntrinsic(push)), 0, {patternIX}), encodingImplicit, InstructionOptions({prefixIX, 0xE5}, {}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(InstructionType::VoidIntrinsic(push)), 0, {patternIY}), encodingImplicit, InstructionOptions({prefixIY, 0xE5}, {}, {}));
        // rr = pop()
        builtins.createInstruction(InstructionSignature(InstructionType(InstructionType::LoadIntrinsic(pop)), 0, {patternBC}), encodingImplicit, InstructionOptions({0xC1}, {}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(InstructionType::LoadIntrinsic(pop)), 0, {patternDE}), encodingImplicit, InstructionOptions({0xD1}, {}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(InstructionType::LoadIntrinsic(pop)), 0, {patternHL}), encodingImplicit, InstructionOptions({0xE1}, {}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(InstructionType::LoadIntrinsic(pop)), 0, {patternAF}), encodingImplicit, InstructionOptions({0xF1}, {}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(InstructionType::LoadIntrinsic(pop)), 0, {patternIX}), encodingImplicit, InstructionOptions({prefixIX, 0xE1}, {}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(InstructionType::LoadIntrinsic(pop)), 0, {patternIY}), encodingImplicit, InstructionOptions({prefixIY, 0xE1}, {}, {}));
        // swap(de, hl)
        // swap(*(sp), hl)
        // swap(*(sp), ix)
        // swap(*(sp), iy)
        builtins.createInstruction(InstructionSignature(InstructionType(InstructionType::VoidIntrinsic(swap)), 0, {patternDE, patternHL}), encodingImplicit, InstructionOptions({0xEB}, {}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(InstructionType::VoidIntrinsic(swap)), 0, {patternIndirectSP, patternHL}), encodingImplicit, InstructionOptions({0xE3}, {}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(InstructionType::VoidIntrinsic(swap)), 0, {patternIndirectSP, patternIX}), encodingImplicit, InstructionOptions({prefixIX, 0xE3}, {}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(InstructionType::VoidIntrinsic(swap)), 0, {patternIndirectSP, patternIY}), encodingImplicit, InstructionOptions({prefixIY, 0xE3}, {}, {}));
        // swap_shadow(af)
        // swap_shadow(bc, de, hl)
        builtins.createInstruction(InstructionSignature(InstructionType(InstructionType::VoidIntrinsic(swap_shadow)), 0, {patternAF}), encodingImplicit, InstructionOptions({0x08}, {}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(InstructionType::VoidIntrinsic(swap_shadow)), 0, {patternBC, patternDE, patternHL}), encodingImplicit, InstructionOptions({0xD9}, {}, {}));
        // ldi
        builtins.createInstruction(InstructionSignature(InstructionType(InstructionType::VoidIntrinsic(load_inc_next)), 0, {patternDE, patternHL, patternBC}), encodingImplicit, InstructionOptions({prefixExtended, 0xA0}, {}, {}));
        // ldir
        builtins.createInstruction(InstructionSignature(InstructionType(InstructionType::VoidIntrinsic(load_inc_repeat)), 0, {patternDE, patternHL, patternBC}), encodingImplicit, InstructionOptions({prefixExtended, 0xB0}, {}, {}));
        // ldd
        builtins.createInstruction(InstructionSignature(InstructionType(InstructionType::VoidIntrinsic(load_dec_next)), 0, {patternDE, patternHL, patternBC}), encodingImplicit, InstructionOptions({prefixExtended, 0xA8}, {}, {}));
        // lddr
        builtins.createInstruction(InstructionSignature(InstructionType(InstructionType::VoidIntrinsic(load_dec_repeat)), 0, {patternDE, patternHL, patternBC}), encodingImplicit, InstructionOptions({prefixExtended, 0xB8}, {}, {}));
        // cpi
        builtins.createInstruction(InstructionSignature(InstructionType(InstructionType::VoidIntrinsic(cmp_inc_next)), 0, {patternHL, patternA, patternBC}), encodingImplicit, InstructionOptions({prefixExtended, 0xA1}, {}, {}));
        // cpir
        builtins.createInstruction(InstructionSignature(InstructionType(InstructionType::VoidIntrinsic(cmp_inc_repeat)), 0, {patternHL, patternA, patternBC}), encodingImplicit, InstructionOptions({prefixExtended, 0xB1}, {}, {}));
        // cpd
        builtins.createInstruction(InstructionSignature(InstructionType(InstructionType::VoidIntrinsic(cmp_dec_next)), 0, {patternHL, patternA, patternBC}), encodingImplicit, InstructionOptions({prefixExtended, 0xA9}, {}, {}));
        // cpdr
        builtins.createInstruction(InstructionSignature(InstructionType(InstructionType::VoidIntrinsic(cmp_dec_repeat)), 0, {patternHL, patternA, patternBC}), encodingImplicit, InstructionOptions({prefixExtended, 0xB9}, {}, {}));
        // 8-bit arithmetic
        {
            using ArithmeticOperatorInfo = std::tuple<InstructionType, std::uint8_t>;
            const ArithmeticOperatorInfo arithmeticOperators[] {
                ArithmeticOperatorInfo {BinaryOperatorKind::Addition, 0x00},
                ArithmeticOperatorInfo {BinaryOperatorKind::AdditionWithCarry, 0x08},
                ArithmeticOperatorInfo {BinaryOperatorKind::Subtraction, 0x10},
                ArithmeticOperatorInfo {BinaryOperatorKind::SubtractionWithCarry, 0x18},
                ArithmeticOperatorInfo {BinaryOperatorKind::BitwiseAnd, 0x20},
                ArithmeticOperatorInfo {BinaryOperatorKind::BitwiseXor, 0x28},
                ArithmeticOperatorInfo {BinaryOperatorKind::BitwiseOr, 0x30},
                ArithmeticOperatorInfo {InstructionType::VoidIntrinsic(cmp), 0x38},
            };
            for (const auto& op : arithmeticOperators) {
                builtins.createInstruction(InstructionSignature(InstructionType(std::get<0>(op)), 0, {patternA, patternImmU8}), encodingU8Operand, InstructionOptions({static_cast<std::uint8_t>(0xC6 | std::get<1>(op))}, {1}, {}));

                for (const auto& sourceRegister : generalRegisters) {
                    std::vector<std::uint8_t> opcode {static_cast<std::uint8_t>(0x80 | std::get<1>(op) | std::get<1>(sourceRegister))};

                    const auto sourceRegisterOperand = std::get<0>(sourceRegister);
                    const auto prefixedSourceRegisterIndex = std::get<2>(sourceRegister);

                    builtins.createInstruction(InstructionSignature(InstructionType(std::get<0>(op)), 0, {patternA, sourceRegisterOperand}), encodingImplicit, InstructionOptions(opcode, {}, {}));                

                    if (prefixedSourceRegisterIndex != SIZE_MAX) {
                        for (const auto& prefixSet : prefixSets) {
                            std::vector<std::uint8_t> prefixedOpcode {std::get<0>(prefixSet)};
                            prefixedOpcode.insert(prefixedOpcode.end(), opcode.begin(), opcode.end());

                            const auto prefixedSource = std::get<1>(prefixSet)[prefixedSourceRegisterIndex];

                            std::vector<std::size_t> prefixedInstructionOptions;
                            auto prefixedEncoding = encodingImplicit;
                            if (prefixedSource->variant.is<InstructionOperandPattern::Index>()) {
                                prefixedInstructionOptions.push_back(1);
                                prefixedEncoding = encodingI8Operand;
                            }

                            builtins.createInstruction(InstructionSignature(InstructionType(std::get<0>(op)), 0, {patternA, prefixedSource}), prefixedEncoding, InstructionOptions(prefixedOpcode, prefixedInstructionOptions, {}));
                        }
                    }
                }
            }
        }    
        // ++r
        // --r
        {
            using IncrementOperatorInfo = std::tuple<InstructionType, std::uint8_t>;
            const IncrementOperatorInfo incrementOperators[] {
                IncrementOperatorInfo {UnaryOperatorKind::PreIncrement, 0x04},
                IncrementOperatorInfo {UnaryOperatorKind::PreDecrement, 0x05},
            };
            for (const auto& op : incrementOperators) {
                for (const auto& sourceRegister : generalRegisters) {
                    std::vector<std::uint8_t> opcode {static_cast<std::uint8_t>(std::get<1>(op) | (std::get<1>(sourceRegister) << 3))};

                    const auto sourceRegisterOperand = std::get<0>(sourceRegister);
                    const auto prefixedSourceRegisterIndex = std::get<2>(sourceRegister);

                    builtins.createInstruction(InstructionSignature(InstructionType(std::get<0>(op)), 0, {sourceRegisterOperand}), encodingImplicit, InstructionOptions(opcode, {}, {zero}));

                    if (prefixedSourceRegisterIndex != SIZE_MAX) {
                        for (const auto& prefixSet : prefixSets) {
                            std::vector<std::uint8_t> prefixedOpcode {std::get<0>(prefixSet)};
                            prefixedOpcode.insert(prefixedOpcode.end(), opcode.begin(), opcode.end());

                            const auto prefixedSource = std::get<1>(prefixSet)[prefixedSourceRegisterIndex];

                            std::vector<std::size_t> prefixedInstructionOptions;
                            if (prefixedSource->variant.is<InstructionOperandPattern::Index>()) {
                                prefixedInstructionOptions.push_back(0);
                            }

                            builtins.createInstruction(InstructionSignature(InstructionType(std::get<0>(op)), 0, {prefixedSource}), encodingI8Operand, InstructionOptions(prefixedOpcode, prefixedInstructionOptions, {zero}));
                        }
                    }
                }
            }
        }
        // daa
        builtins.createInstruction(InstructionSignature(InstructionType(InstructionType::VoidIntrinsic(decimal_adjust)), 0, {}), encodingImplicit, InstructionOptions({0x27}, {}, {}));
        // exx
        builtins.createInstruction(InstructionSignature(InstructionType(InstructionType::VoidIntrinsic(exchange_16_bit_registers)), 0, {}), encodingImplicit, InstructionOptions({0xD9}, {}, {}));
		// a = ~a
        builtins.createInstruction(InstructionSignature(InstructionType(UnaryOperatorKind::BitwiseNegation), 0, {patternA}), encodingImplicit, InstructionOptions({0x2F}, {}, {}));
        // a = -a
        builtins.createInstruction(InstructionSignature(InstructionType(UnaryOperatorKind::SignedNegation), 0, {patternA}), encodingImplicit, InstructionOptions({prefixExtended, 0x44}, {}, {}));
        // carry = false
        // carry = true
        // carry = !carry
        builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Assignment), 0, {patternCarry, patternFalse}), encodingImplicit, InstructionOptions({0x37, 0x3F}, {}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Assignment), 0, {patternCarry, patternTrue}), encodingImplicit, InstructionOptions({0x37}, {}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(UnaryOperatorKind::LogicalNegation), 0, {patternCarry}), encodingImplicit, InstructionOptions({0x3F}, {}, {}));
        // nop
        builtins.createInstruction(InstructionSignature(InstructionType(InstructionType::VoidIntrinsic(nop)), 0, {}), encodingImplicit, InstructionOptions({0x00}, {}, {}));
        // halt
        builtins.createInstruction(InstructionSignature(InstructionType(InstructionType::VoidIntrinsic(halt)), 0, {}), encodingImplicit, InstructionOptions({0x76}, {}, {}));
        // interrupt = false
        // interrupt = true
        builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Assignment), 0, {patternInterrupt, patternFalse}), encodingImplicit, InstructionOptions({0xF3}, {}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Assignment), 0, {patternInterrupt, patternTrue}), encodingImplicit, InstructionOptions({0xFB}, {}, {}));
        // interrupt_mode = 0
        // interrupt_mode = 1
        // interrupt_mode = 2
        builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Assignment), 0, {patternInterruptMode, pattern0}), encodingImplicit, InstructionOptions({prefixExtended, 0x46}, {}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Assignment), 0, {patternInterruptMode, pattern1}), encodingImplicit, InstructionOptions({prefixExtended, 0x56}, {}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Assignment), 0, {patternInterruptMode, pattern2}), encodingImplicit, InstructionOptions({prefixExtended, 0x5E}, {}, {}));
        // 16-bit arithmetic
        {
            using ArithmeticOperator = std::tuple<InstructionType, std::vector<std::uint8_t>>;
            const ArithmeticOperator arithmeticOperators[] {
                ArithmeticOperator {BinaryOperatorKind::Addition, {0x09}},
                ArithmeticOperator {BinaryOperatorKind::AdditionWithCarry, {prefixExtended, 0x4A}},
                ArithmeticOperator {BinaryOperatorKind::Subtraction, {0x37, 0x3F, prefixExtended, 0x42}},
                ArithmeticOperator {BinaryOperatorKind::SubtractionWithCarry, {prefixExtended, 0x42}},
            };
            for (const auto& op : arithmeticOperators) {
                for (const auto& reg : generalRegisterPairs) {
                    auto opcode = std::get<1>(op);
                    opcode[opcode.size() - 1] |= (std::get<1>(reg) << 4);
                    builtins.createInstruction(InstructionSignature(InstructionType(std::get<0>(op)), 0, {patternHL, std::get<0>(reg)}), encodingImplicit, InstructionOptions(opcode, {}, {}));
                }
            }
        }
        // ix += rr
        // iy += rr
        builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Addition), 0, {patternIX, patternBC}), encodingImplicit, InstructionOptions({prefixIX, 0x09}, {}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Addition), 0, {patternIX, patternDE}), encodingImplicit, InstructionOptions({prefixIX, 0x19}, {}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Addition), 0, {patternIX, patternIX}), encodingImplicit, InstructionOptions({prefixIX, 0x29}, {}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Addition), 0, {patternIX, patternSP}), encodingImplicit, InstructionOptions({prefixIX, 0x39}, {}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Addition), 0, {patternIY, patternBC}), encodingImplicit, InstructionOptions({prefixIY, 0x09}, {}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Addition), 0, {patternIY, patternDE}), encodingImplicit, InstructionOptions({prefixIY, 0x19}, {}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Addition), 0, {patternIY, patternIY}), encodingImplicit, InstructionOptions({prefixIY, 0x29}, {}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Addition), 0, {patternIY, patternSP}), encodingImplicit, InstructionOptions({prefixIY, 0x39}, {}, {}));
        // ++rr
        builtins.createInstruction(InstructionSignature(InstructionType(UnaryOperatorKind::PreIncrement), 0, {patternBC}), encodingImplicit, InstructionOptions({0x03}, {}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(UnaryOperatorKind::PreIncrement), 0, {patternDE}), encodingImplicit, InstructionOptions({0x13}, {}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(UnaryOperatorKind::PreIncrement), 0, {patternHL}), encodingImplicit, InstructionOptions({0x23}, {}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(UnaryOperatorKind::PreIncrement), 0, {patternSP}), encodingImplicit, InstructionOptions({0x33}, {}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(UnaryOperatorKind::PreIncrement), 0, {patternIX}), encodingImplicit, InstructionOptions({prefixIX, 0x23}, {}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(UnaryOperatorKind::PreIncrement), 0, {patternIY}), encodingImplicit, InstructionOptions({prefixIY, 0x23}, {}, {}));
        // --rr
        builtins.createInstruction(InstructionSignature(InstructionType(UnaryOperatorKind::PreDecrement), 0, {patternBC}), encodingImplicit, InstructionOptions({0x0B}, {}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(UnaryOperatorKind::PreDecrement), 0, {patternDE}), encodingImplicit, InstructionOptions({0x1B}, {}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(UnaryOperatorKind::PreDecrement), 0, {patternHL}), encodingImplicit, InstructionOptions({0x2B}, {}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(UnaryOperatorKind::PreDecrement), 0, {patternSP}), encodingImplicit, InstructionOptions({0x3B}, {}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(UnaryOperatorKind::PreDecrement), 0, {patternIX}), encodingImplicit, InstructionOptions({prefixIX, 0x2B}, {}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(UnaryOperatorKind::PreDecrement), 0, {patternIY}), encodingImplicit, InstructionOptions({prefixIY, 0x2B}, {}, {}));
        // bitshifts
        {
            const std::pair<InstructionType, std::vector<std::uint8_t>> shiftOperators[] {
                {BinaryOperatorKind::LeftShift, {prefixBit, 0x20}}, // sla
                {BinaryOperatorKind::LogicalLeftShift, {prefixBit, 0x20}}, // sla
                {BinaryOperatorKind::RightShift, {prefixBit, 0x28}}, // sra
                {BinaryOperatorKind::LogicalRightShift, {prefixBit, 0x38}}, // srl
                {BinaryOperatorKind::LeftRotate, {prefixBit, 0x00}}, // rlc
                {BinaryOperatorKind::RightRotate, {prefixBit, 0x08}}, // rrc
                {BinaryOperatorKind::LeftRotateWithCarry, {prefixBit, 0x10}}, // rl
                {BinaryOperatorKind::RightRotateWithCarry, {prefixBit, 0x18}}, // rr
            };
            for (const auto& op : shiftOperators) {
                for (const auto& sourceRegister : generalRegisters) {
                    auto binKind = std::get<0>(op);
                    auto opcode = std::get<1>(op);
                    opcode[opcode.size() - 1] |= std::get<1>(sourceRegister);

                    const auto sourceRegisterOperand = std::get<0>(sourceRegister);
                    const auto prefixedSourceRegisterIndex = std::get<2>(sourceRegister);

                    bool match = false;
                    if (const auto reg = sourceRegisterOperand->variant.tryGet<InstructionOperandPattern::Register>()) {
                        if (reg->definition == a && (binKind == BinaryOperatorKind::LeftShift || binKind == BinaryOperatorKind::LogicalLeftShift)) {
                            builtins.createInstruction(InstructionSignature(binKind, 0, {sourceRegisterOperand, patternImmU8}), encodingRepeatedImplicit, InstructionOptions({0x87}, {1}, {}));
                            match = true;
                        }
                    }

                    if (!match) {
                        builtins.createInstruction(InstructionSignature(binKind, 0, {sourceRegisterOperand, patternImmU8}), encodingRepeatedImplicit, InstructionOptions(opcode, {1}, {}));
                    }

                    if (prefixedSourceRegisterIndex != SIZE_MAX) {
                        for (const auto& prefixSet : prefixSets) {
                            std::vector<std::uint8_t> prefixedOpcode {std::get<0>(prefixSet)};
                            prefixedOpcode.insert(prefixedOpcode.end(), opcode.begin(), opcode.end());

                            const auto prefixedSource = std::get<1>(prefixSet)[prefixedSourceRegisterIndex];

                            std::vector<std::size_t> prefixedInstructionOptions;
                            auto prefixedEncoding = encodingRepeatedImplicit;
                            if (prefixedSource->variant.is<InstructionOperandPattern::Index>()) {
                                prefixedInstructionOptions.push_back(0);
                                prefixedInstructionOptions.push_back(1);
                                prefixedEncoding = encodingRepeatedI8Operand;
                            } else {
                                prefixedInstructionOptions.push_back(1);
                            }

                            builtins.createInstruction(InstructionSignature(binKind, 0, {prefixedSource, patternImmU8}), prefixedEncoding, InstructionOptions(prefixedOpcode, prefixedInstructionOptions, {}));
                        }
                    }
                }
            }
        }
        // hl <<= n
        builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::LeftShift), 0, {patternHL, patternImmU8}), encodingRepeatedImplicit, InstructionOptions({0x29}, {1}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::LogicalLeftShift), 0, {patternHL, patternImmU8}), encodingRepeatedImplicit, InstructionOptions({0x29}, {1}, {}));
        // rld, rrd
        builtins.createInstruction(InstructionSignature(InstructionType(InstructionType::VoidIntrinsic(rotate_left_digits)), 0, {patternA, patternHL}), encodingImplicit, InstructionOptions({prefixExtended, 0x6F}, {}, {}));
        builtins.createInstruction(InstructionSignature(InstructionType(InstructionType::VoidIntrinsic(rotate_right_digits)), 0, {patternA, patternHL}), encodingImplicit, InstructionOptions({prefixExtended, 0x67}, {}, {}));

        // bit(r $ n)
        // r $ n = true
        // r $ n = false
        for (const auto& reg : generalRegisters) {
            auto patternRegisterBit = builtins.createInstructionOperandPattern(InstructionOperandPattern::BitIndex(
                std::get<0>(reg)->clone(),
                makeFwdUnique<InstructionOperandPattern>(InstructionOperandPattern::Capture(
                    patternImmBitSubscript->clone()))));

            builtins.createInstruction(InstructionSignature(InstructionType(InstructionType::VoidIntrinsic(bit)), 0, {std::get<0>(reg), patternImmBitSubscript}), encodingBitIndex, InstructionOptions({prefixBit, static_cast<std::uint8_t>(0x40 | std::get<1>(reg))}, {1, 0}, {}));
            builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Assignment), 0, {patternRegisterBit, patternFalse}), encodingBitIndex, InstructionOptions({prefixBit, static_cast<std::uint8_t>(0x80 | std::get<1>(reg))}, {0, 0}, {}));
            builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Assignment), 0, {patternRegisterBit, patternTrue}), encodingBitIndex, InstructionOptions({prefixBit, static_cast<std::uint8_t>(0xC0 | std::get<1>(reg))}, {0, 0}, {}));

            if (std::get<0>(reg) == patternIndirectHL) {
                auto patternIndexIXBit = builtins.createInstructionOperandPattern(InstructionOperandPattern::BitIndex(
                    patternIndexIX->clone(),
                    makeFwdUnique<InstructionOperandPattern>(InstructionOperandPattern::Capture(
                        patternImmBitSubscript->clone()))));
                builtins.createInstruction(InstructionSignature(InstructionType(InstructionType::VoidIntrinsic(bit)), 0, {patternIndexIX, patternImmBitSubscript}), encodingBitIndexI8Operand, InstructionOptions({prefixIX, prefixBit, static_cast<std::uint8_t>(0x40 | std::get<1>(reg))}, {0, 1, 0}, {}));
                builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Assignment), 0, {patternIndexIXBit, patternFalse}), encodingBitIndexI8Operand, InstructionOptions({prefixIX, prefixBit, static_cast<std::uint8_t>(0x80 | std::get<1>(reg))}, {0, 0, 1}, {}));
                builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Assignment), 0, {patternIndexIXBit, patternTrue}), encodingBitIndexI8Operand, InstructionOptions({prefixIX, prefixBit, static_cast<std::uint8_t>(0xC0 | std::get<1>(reg))}, {0, 0, 1}, {}));

                auto patternIndexIYBit = builtins.createInstructionOperandPattern(InstructionOperandPattern::BitIndex(
                    patternIndexIY->clone(),
                    makeFwdUnique<InstructionOperandPattern>(InstructionOperandPattern::Capture(
                        patternImmBitSubscript->clone()))));
                builtins.createInstruction(InstructionSignature(InstructionType(InstructionType::VoidIntrinsic(bit)), 0, {patternIndexIY, patternImmBitSubscript}), encodingBitIndexI8Operand, InstructionOptions({prefixIY, prefixBit, static_cast<std::uint8_t>(0x40 | std::get<1>(reg))}, {0, 1, 0}, {}));
                builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Assignment), 0, {patternIndexIYBit, patternFalse}), encodingBitIndexI8Operand, InstructionOptions({prefixIY, prefixBit, static_cast<std::uint8_t>(0x80 | std::get<1>(reg))}, {0, 0, 1}, {}));
                builtins.createInstruction(InstructionSignature(InstructionType(BinaryOperatorKind::Assignment), 0, {patternIndexIYBit, patternTrue}), encodingBitIndexI8Operand, InstructionOptions({prefixIY, prefixBit, static_cast<std::uint8_t>(0xC0 | std::get<1>(reg))}, {0, 0, 1}, {}));
            }
        }

        // jr abs
        builtins.createInstruction(InstructionSignature(BranchKind::Goto, 0, {patternAtLeast0, patternImmU16}), encodingPCRelativeI8Operand, InstructionOptions({0x18}, {1}, {}));
        // jr cond
        builtins.createInstruction(InstructionSignature(BranchKind::Goto, 0, {patternAtLeast0, patternImmU16, patternZero, patternFalse}), encodingPCRelativeI8Operand, InstructionOptions({0x20}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Goto, 0, {patternAtLeast0, patternImmU16, patternZero, patternTrue}), encodingPCRelativeI8Operand, InstructionOptions({0x28}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Goto, 0, {patternAtLeast0, patternImmU16, patternCarry, patternFalse}), encodingPCRelativeI8Operand, InstructionOptions({0x30}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Goto, 0, {patternAtLeast0, patternImmU16, patternCarry, patternTrue}), encodingPCRelativeI8Operand, InstructionOptions({0x38}, {1}, {}));
        // jp abs
        builtins.createInstruction(InstructionSignature(BranchKind::Goto, 0, {patternAtLeast1, patternImmU16}), encodingU16Operand, InstructionOptions({0xC3}, {1}, {}));
        // jp cond, abs
        builtins.createInstruction(InstructionSignature(BranchKind::Goto, 0, {patternAtLeast1, patternImmU16, patternZero, patternFalse}), encodingU16Operand, InstructionOptions({0xC2}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Goto, 0, {patternAtLeast1, patternImmU16, patternZero, patternTrue}), encodingU16Operand, InstructionOptions({0xCA}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Goto, 0, {patternAtLeast1, patternImmU16, patternCarry, patternFalse}), encodingU16Operand, InstructionOptions({0xD2}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Goto, 0, {patternAtLeast1, patternImmU16, patternCarry, patternTrue}), encodingU16Operand, InstructionOptions({0xDA}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Goto, 0, {patternAtLeast0, patternImmU16, patternOverflow, patternFalse}), encodingU16Operand, InstructionOptions({0xE2}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Goto, 0, {patternAtLeast0, patternImmU16, patternOverflow, patternTrue}), encodingU16Operand, InstructionOptions({0xEA}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Goto, 0, {patternAtLeast0, patternImmU16, patternNegative, patternFalse}), encodingU16Operand, InstructionOptions({0xF2}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Goto, 0, {patternAtLeast0, patternImmU16, patternNegative, patternTrue}), encodingU16Operand, InstructionOptions({0xFA}, {1}, {}));
        // jp hl
        // jp ix
        // jp iy
        builtins.createInstruction(InstructionSignature(BranchKind::Goto, 0, {patternAtLeast0, patternHL}), encodingImplicit, InstructionOptions({0xE9}, {}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Goto, 0, {patternAtLeast0, patternIX}), encodingImplicit, InstructionOptions({prefixIX, 0xE9}, {}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Goto, 0, {patternAtLeast0, patternIY}), encodingImplicit, InstructionOptions({prefixIY, 0xE9}, {}, {}));
        // djnz
        builtins.createInstruction(InstructionSignature(InstructionType::VoidIntrinsic(dec_branch_not_zero), 0, {patternB, patternImmU16}), encodingPCRelativeI8Operand, InstructionOptions({0x10}, {1}, {}));
        // call abs
        builtins.createInstruction(InstructionSignature(BranchKind::Call, 0, {patternAtLeast0, patternImmU16}), encodingU16Operand, InstructionOptions({0xCD}, {1}, {}));
        // call cond, abs
        builtins.createInstruction(InstructionSignature(BranchKind::Call, 0, {patternAtLeast0, patternImmU16, patternZero, patternFalse}), encodingU16Operand, InstructionOptions({0xC4}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Call, 0, {patternAtLeast0, patternImmU16, patternZero, patternTrue}), encodingU16Operand, InstructionOptions({0xCC}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Call, 0, {patternAtLeast0, patternImmU16, patternCarry, patternFalse}), encodingU16Operand, InstructionOptions({0xD4}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Call, 0, {patternAtLeast0, patternImmU16, patternCarry, patternTrue}), encodingU16Operand, InstructionOptions({0xDC}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Call, 0, {patternAtLeast0, patternImmU16, patternOverflow, patternFalse}), encodingU16Operand, InstructionOptions({0xE4}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Call, 0, {patternAtLeast0, patternImmU16, patternOverflow, patternTrue}), encodingU16Operand, InstructionOptions({0xEC}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Call, 0, {patternAtLeast0, patternImmU16, patternNegative, patternFalse}), encodingU16Operand, InstructionOptions({0xF4}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Call, 0, {patternAtLeast0, patternImmU16, patternNegative, patternTrue}), encodingU16Operand, InstructionOptions({0xFC}, {1}, {}));
        // ret
        builtins.createInstruction(InstructionSignature(BranchKind::Return, 0, {patternAtLeast0}), encodingImplicit, InstructionOptions({0xC9}, {1}, {}));
        // ret cond
        builtins.createInstruction(InstructionSignature(BranchKind::Return, 0, {patternAtLeast0, patternZero, patternFalse}), encodingImplicit, InstructionOptions({0xC0}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Return, 0, {patternAtLeast0, patternZero, patternTrue}), encodingImplicit, InstructionOptions({0xC8}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Return, 0, {patternAtLeast0, patternCarry, patternFalse}), encodingImplicit, InstructionOptions({0xD0}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Return, 0, {patternAtLeast0, patternCarry, patternTrue}), encodingImplicit, InstructionOptions({0xD8}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Return, 0, {patternAtLeast0, patternOverflow, patternFalse}), encodingImplicit, InstructionOptions({0xE0}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Return, 0, {patternAtLeast0, patternOverflow, patternTrue}), encodingImplicit, InstructionOptions({0xE8}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Return, 0, {patternAtLeast0, patternNegative, patternFalse}), encodingImplicit, InstructionOptions({0xF0}, {1}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Return, 0, {patternAtLeast0, patternNegative, patternTrue}), encodingImplicit, InstructionOptions({0xF8}, {1}, {}));
        // reti
        builtins.createInstruction(InstructionSignature(BranchKind::IrqReturn, 0, {patternAtLeast0}), encodingImplicit, InstructionOptions({prefixExtended, 0x4D}, {}, {}));
        // retn
        builtins.createInstruction(InstructionSignature(BranchKind::NmiReturn, 0, {patternAtLeast0}), encodingImplicit, InstructionOptions({prefixExtended, 0x45}, {}, {}));
        // rst n
        builtins.createInstruction(InstructionSignature(BranchKind::Call, 0, {patternAtLeast0, pattern0}), encodingImplicit, InstructionOptions({0xC7}, {0}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Call, 0, {patternAtLeast0, pattern8}), encodingImplicit, InstructionOptions({0xCF}, {0}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Call, 0, {patternAtLeast0, pattern16}), encodingImplicit, InstructionOptions({0xD7}, {0}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Call, 0, {patternAtLeast0, pattern24}), encodingImplicit, InstructionOptions({0xDF}, {0}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Call, 0, {patternAtLeast0, pattern32}), encodingImplicit, InstructionOptions({0xE7}, {0}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Call, 0, {patternAtLeast0, pattern40}), encodingImplicit, InstructionOptions({0xEF}, {0}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Call, 0, {patternAtLeast0, pattern48}), encodingImplicit, InstructionOptions({0xF7}, {0}, {}));
        builtins.createInstruction(InstructionSignature(BranchKind::Call, 0, {patternAtLeast0, pattern56}), encodingImplicit, InstructionOptions({0xFF}, {0}, {}));

        // in a, (n)
        builtins.createInstruction(InstructionSignature(InstructionType::LoadIntrinsic(io_read), 0, {patternA, patternImmU8}), encodingU8Operand, InstructionOptions({0xDB}, {1}, {}));
        // in r, (c)
        for (const auto& reg : generalRegisters) {
            if (std::get<0>(reg) != patternIndirectHL) {
                builtins.createInstruction(InstructionSignature(InstructionType::LoadIntrinsic(io_read), 0, {std::get<0>(reg), patternC}), encodingImplicit, InstructionOptions({prefixExtended, static_cast<std::uint8_t>(std::get<1>(reg) * 8)}, {}, {}));
            }
        }
        // ini
        builtins.createInstruction(InstructionSignature(InstructionType::VoidIntrinsic(io_read_inc_next), 0, {patternHL, patternC, patternB}), encodingImplicit, InstructionOptions({prefixExtended, 0xA2}, {}, {}));
        // inir
        builtins.createInstruction(InstructionSignature(InstructionType::VoidIntrinsic(io_read_inc_repeat), 0, {patternHL, patternC, patternB}), encodingImplicit, InstructionOptions({prefixExtended, 0xB2}, {}, {}));
        // ind
        builtins.createInstruction(InstructionSignature(InstructionType::VoidIntrinsic(io_read_dec_next), 0, {patternHL, patternC, patternB}), encodingImplicit, InstructionOptions({prefixExtended, 0xAA}, {}, {}));
        // indr
        builtins.createInstruction(InstructionSignature(InstructionType::VoidIntrinsic(io_read_dec_repeat), 0, {patternHL, patternC, patternB}), encodingImplicit, InstructionOptions({prefixExtended, 0xBA}, {}, {}));
        // out (n), a
        builtins.createInstruction(InstructionSignature(InstructionType::VoidIntrinsic(io_write), 0, {patternImmU8, patternA}), encodingU8Operand, InstructionOptions({0xD3}, {0}, {}));
        // out (c), r
        for (const auto& reg : generalRegisters) {
            if (std::get<0>(reg) != patternIndirectHL) {
                builtins.createInstruction(InstructionSignature(InstructionType::VoidIntrinsic(io_write), 0, {patternC, std::get<0>(reg)}), encodingImplicit, InstructionOptions({prefixExtended, static_cast<std::uint8_t>(std::get<1>(reg) * 8 + 0x01)}, {}, {}));
            }
        }
        // outi
        builtins.createInstruction(InstructionSignature(InstructionType::VoidIntrinsic(io_write_inc_next), 0, {patternC, patternHL, patternB}), encodingImplicit, InstructionOptions({prefixExtended, 0xA3}, {}, {}));
        // outir
        builtins.createInstruction(InstructionSignature(InstructionType::VoidIntrinsic(io_write_inc_repeat), 0, {patternC, patternHL, patternB}), encodingImplicit, InstructionOptions({prefixExtended, 0xB3}, {}, {}));
        // outd
        builtins.createInstruction(InstructionSignature(InstructionType::VoidIntrinsic(io_write_dec_next), 0, {patternC, patternHL, patternB}), encodingImplicit, InstructionOptions({prefixExtended, 0xAB}, {}, {}));
        // outdr
        builtins.createInstruction(InstructionSignature(InstructionType::VoidIntrinsic(io_write_dec_repeat), 0, {patternC, patternHL, patternB}), encodingImplicit, InstructionOptions({prefixExtended, 0xBB}, {}, {}));
    }

    Definition* Z80Platform::getPointerSizedType() const {
        return pointerSizedType;
    }

    Definition* Z80Platform::getFarPointerSizedType() const {
        return farPointerSizedType;
    }

    std::unique_ptr<PlatformTestAndBranch> Z80Platform::getTestAndBranch(const Compiler& compiler, const Definition* type, BinaryOperatorKind op, const Expression* left, const Expression* right, std::size_t distanceHint) const {
        static_cast<void>(compiler);
        static_cast<void>(distanceHint);

        switch (op) {
            case BinaryOperatorKind::Equal:
            case BinaryOperatorKind::NotEqual: {
                if (const auto leftUnary = left->variant.tryGet<Expression::UnaryOperator>()) {
                    const auto innerOp = leftUnary->op;
                    const auto innerOperand = leftUnary->operand.get();

                    // --b != 0 -> decrement_branch_not_zero(b, dest)
                    if (innerOp == UnaryOperatorKind::PreDecrement) {
                        if (const auto decrementedRegister = innerOperand->variant.tryGet<Expression::ResolvedIdentifier>()) {
                            if (decrementedRegister->definition == b) {
                                if (const auto outerRightImmediate = right->variant.tryGet<Expression::IntegerLiteral>()) {
                                    if (op == BinaryOperatorKind::NotEqual && outerRightImmediate->value.isZero()) {
                                        return std::make_unique<PlatformTestAndBranch>(
                                            InstructionType::VoidIntrinsic(dec_branch_not_zero),
                                            std::vector<const Expression*> {innerOperand},
                                            std::vector<PlatformBranch> {}
                                        );
                                    }
                                }
                            }
                        }
                    }
                }

                if (const auto leftRegister = left->variant.tryGet<Expression::ResolvedIdentifier>()) {
                    if (leftRegister->definition == a) {
                        // a == 0 -> { a |= a; } && zero
                        if (const auto rightImmediate = right->variant.tryGet<Expression::IntegerLiteral>()) {
                            if (rightImmediate->value.isZero()) {
                                return std::make_unique<PlatformTestAndBranch>(
                                    BinaryOperatorKind::BitwiseOr,
                                    std::vector<const Expression*> {left, left},
                                    std::vector<PlatformBranch> { PlatformBranch(zero, op == BinaryOperatorKind::Equal, true) }
                                );
                            }
                        }

                        // a == right -> { cmp(a, right); } && zero
                        return std::make_unique<PlatformTestAndBranch>(
                            InstructionType::VoidIntrinsic(cmp),
                            std::vector<const Expression*> {left, right},
                            std::vector<PlatformBranch> { PlatformBranch(zero, op == BinaryOperatorKind::Equal, true) }
                        );
                    }
                }

                return nullptr;
            }
            case BinaryOperatorKind::LessThan:
            case BinaryOperatorKind::GreaterThanOrEqual: {
                if (const auto integerType = type->variant.tryGet<Definition::BuiltinIntegerType>()) {
                    if (integerType->min.isNegative()) {
                        if (const auto rightImmediate = right->variant.tryGet<Expression::IntegerLiteral>()) {
                            if (rightImmediate->value.isZero()) {
                                if (const auto leftRegister = left->variant.tryGet<Expression::ResolvedIdentifier>()) {
                                    if (leftRegister->definition == a) {
                                        // a < 0 -> { cmp(a, 0); } && negative
                                        // a >= 0 -> { cmp(a, 0); } && !negative
                                        return std::make_unique<PlatformTestAndBranch>(
                                            InstructionType::VoidIntrinsic(cmp),
                                            std::vector<const Expression*> {left, right},
                                            std::vector<PlatformBranch> { PlatformBranch(negative, op == BinaryOperatorKind::LessThan, true) }
                                        );
                                    } else {
                                        // left < 0 -> { bit(left, 7); } && !zero
                                        // left >= 0 -> { bit(left, 7); } && zero
                                        std::vector<InstructionOperandRoot> operandRoots;
                                        operandRoots.push_back(InstructionOperandRoot(left, compiler.createOperandFromExpression(left, true)));
                                        operandRoots.push_back(InstructionOperandRoot(bitIndex7Expression.get(), compiler.createOperandFromExpression(bitIndex7Expression.get(), true)));

                                        if (compiler.getBuiltins().selectInstruction(InstructionType::VoidIntrinsic(bit), 0, operandRoots)) {
                                            return std::make_unique<PlatformTestAndBranch>(
                                                InstructionType::VoidIntrinsic(bit),
                                                std::vector<const Expression*> {left, bitIndex7Expression.get()},
                                                std::vector<PlatformBranch> { PlatformBranch(zero, op != BinaryOperatorKind::LessThan, true) }
                                            );
                                        }
                                    }
                                }
                            }
                        }           
                    } else {
                        // a < right -> { cmp(a, right); } && carry
                        // a >= right -> { cmp(a, right); } && carry
                        if (const auto leftRegister = left->variant.tryGet<Expression::ResolvedIdentifier>()) {
                            if (leftRegister->definition == a) {
                                return std::make_unique<PlatformTestAndBranch>(
                                    InstructionType::VoidIntrinsic(cmp),
                                    std::vector<const Expression*> {left, right},
                                    std::vector<PlatformBranch> { PlatformBranch(carry, op == BinaryOperatorKind::LessThan, true) }
                                );
                            }
                        }
                    }
                }

                return nullptr;
            }
            case BinaryOperatorKind::LessThanOrEqual: {
                if (const auto integerType = type->variant.tryGet<Definition::BuiltinIntegerType>()) {
                    if (integerType->min.isNegative()) {
                        // a <= right -> { cmp(a, right); } && (zero || negative)
                        if (const auto leftRegister = left->variant.tryGet<Expression::ResolvedIdentifier>()) {
                            if (leftRegister->definition == a) {
                                if (const auto rightImmediate = right->variant.tryGet<Expression::IntegerLiteral>()) {
                                    if (rightImmediate->value.isZero()) {
                                        return std::make_unique<PlatformTestAndBranch>(
                                            InstructionType::VoidIntrinsic(cmp),
                                            std::vector<const Expression*> {left, right},
                                            std::vector<PlatformBranch> {
                                                PlatformBranch(zero, true, true),
                                                PlatformBranch(negative, true, true)
                                            }
                                        );
                                    }
                                }
                            }
                        }
                    } else {
                        // a <= right -> { cmp(a, right); } && (zero || carry)
                        if (const auto leftRegister = left->variant.tryGet<Expression::ResolvedIdentifier>()) {
                            if (leftRegister->definition == a) {
                                return std::make_unique<PlatformTestAndBranch>(
                                    InstructionType::VoidIntrinsic(cmp),
                                    std::vector<const Expression*> {left, right},
                                    std::vector<PlatformBranch> {
                                        PlatformBranch(zero, true, true),
                                        PlatformBranch(carry, true, true)
                                    }
                                );
                            }
                        }
                    }
                }                

                return nullptr;
            }
            case BinaryOperatorKind::GreaterThan: {
                if (const auto integerType = type->variant.tryGet<Definition::BuiltinIntegerType>()) {
                    if (integerType->min.isNegative()) {
                        // a > right -> { cmp(a, right); } && !zero && !negative
                        if (const auto leftRegister = left->variant.tryGet<Expression::ResolvedIdentifier>()) {
                            if (leftRegister->definition == a) {
                                if (const auto rightImmediate = right->variant.tryGet<Expression::IntegerLiteral>()) {
                                    if (rightImmediate->value.isZero()) {
                                        return std::make_unique<PlatformTestAndBranch>(
                                            InstructionType::VoidIntrinsic(cmp),
                                            std::vector<const Expression*> {left, right},
                                            std::vector<PlatformBranch> {
                                                PlatformBranch(zero, true, false),
                                                PlatformBranch(negative, false, true)
                                            }
                                        );
                                    }
                                }
                            }
                        }
                    } else {
                        // a > right -> { cmp(a, right); } && !zero && !carry
                        if (const auto leftRegister = left->variant.tryGet<Expression::ResolvedIdentifier>()) {
                            if (leftRegister->definition == a) {
                                return std::make_unique<PlatformTestAndBranch>(
                                    InstructionType::VoidIntrinsic(cmp),
                                    std::vector<const Expression*> {left, right},
                                    std::vector<PlatformBranch> {
                                        PlatformBranch(zero, true, false),
                                        PlatformBranch(carry, false, true)
                                    }
                                );
                            }
                        }
                    }
                }

                return nullptr;
            }
            case BinaryOperatorKind::BitIndexing: {
                // left $ right -> { bit(left, right); } && !zero
                if (const auto leftRegister = left->variant.tryGet<Expression::ResolvedIdentifier>()) {
                    if (leftRegister->definition->variant.is<Definition::BuiltinRegister>()) {
                        if (const auto rightInteger = right->variant.tryGet<Expression::IntegerLiteral>()) {
                            const auto bitIndex = rightInteger->value;

                            if (Int128(0) <= bitIndex && bitIndex <= Int128(7)) {
                                return std::make_unique<PlatformTestAndBranch>(
                                    InstructionType::VoidIntrinsic(bit),
                                    std::vector<const Expression*> {left, right},
                                    std::vector<PlatformBranch> { PlatformBranch(zero, false, true) }
                                );
                            }
                        }
                    }
                }

                return nullptr;
            }
            default: return nullptr;
        }
    }

    Definition* Z80Platform::getZeroFlag() const {
        return zero;
    }

    Int128 Z80Platform::getPlaceholderValue() const {
        return Int128(UINT64_C(0xCCCCCCCCCCCCCCCC));
    }
}