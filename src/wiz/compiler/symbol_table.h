#ifndef WIZ_COMPILER_SYMBOL_TABLE_H
#define WIZ_COMPILER_SYMBOL_TABLE_H

#include <set>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include <wiz/utility/fwd_unique_ptr.h>

namespace wiz {
    struct Definition;
    class Report;

    class SymbolTable {
        public:
            static std::string generateBlockName();

            SymbolTable();
            SymbolTable(SymbolTable* parent, StringView namespaceName);
            ~SymbolTable();

            SymbolTable* getParent() const;
            std::string getFullName() const;
            void printKeys(Report* report) const;

            void getDefinitions(std::vector<Definition*>& results) const;
            void getDefinitions(std::vector<const Definition*>& results) const;

            Definition* addDefinition(Report* report, FwdUniquePtr<Definition> def);

            template <typename... Args>
            Definition* createDefinition(Report* report, Args&&... args) {
                auto definition = makeFwdUnique<Definition>(std::forward<Args>(args)...);
                return addDefinition(report, std::move(definition));
            }

            bool addImport(SymbolTable* scope);
            bool addRecursiveImport(SymbolTable* scope);
            Definition* findLocalMemberDefinition(StringView name) const;
            void findImportedMemberDefinitions(StringView name, std::set<Definition*>& results) const;
            void findMemberDefinitions(StringView name, std::set<Definition*>& results) const;
            void findUnqualifiedDefinitions(StringView name, std::set<Definition*>& results) const;

        private:
            SymbolTable* parent;
            StringView namespaceName;
            std::vector<SymbolTable*> imports;
            std::unordered_map<StringView, FwdUniquePtr<Definition>> namesToDefinitions;
    };
}

#endif
