# AnyPS5: exploração Metal no macOS

Data da investigação: 16 de outubro de 2026 (America/Sao_Paulo).

## Escopo e evidência

Checkout exclusivo: `/Users/ycarogabriel/Downloads/pessoal/Any_PS5-metal`, branch `metal/explore`. Base: `c6dfbc0a3c869b36449ab3cd885a0fd3890d0b33`. Não houve push nem edição no checkout `../Any_PS5`.

Ambiente observado: Apple M1, 16 GiB, macOS 26.6.2 (25G83), AppleClang 21.0.0.21000334. CMake configura os binários para x86-64; o caminho avaliado é guest/runtime x86-64 sob Rosetta, Vulkan traduzido por MoltenVK. SDK solicitado: `~/VulkanSDK/1.4.363.0/macOS`.

## Leitura

- [Build, testes e classificação de falhas](gpu-failures.md).
- [Custo e alcance do backend Metal de RINNECODER](native-metal.md).
- [KosmicKrisp, FEXCore e AOT](native-cpu-kosmickrisp.md).

Os resultados locais de execução são separados dos relatos dos autores e das estimativas de engenharia. Os snapshots externos têm revisão registrada nos documentos; branches, PRs e versões podem mudar depois desta coleta. Não foram executados jogos nem testes de apresentação com janela.
