# CPU nativa e KosmicKrisp: caminhos distintos

Consulta em 16/10/2026, America/Sao_Paulo. Esta análise leu fontes e metadados; não compilou FEX/AOT, não executou fixtures, GPU ou janelas e não instalou dependências. Os resultados dos autores abaixo não são resultados locais.

## Conclusão

KosmicKrisp é um candidato a backend Vulkan para um processo ARM64, mas trocar o ICD não transforma o runner x86-64 atual em ARM64. O SDK local contém `libvulkan_kosmickrisp.dylib` somente ARM64, segundo `file`, e o manifesto `libkosmickrisp_icd.json` anuncia API 1.4.0. Isso comprova presença e arquitetura, não funcionamento nem compatibilidade com os requisitos gráficos do AnyPS5.

O caminho FEX mais útil para investigação futura é o runner da PR #4, com a HLE ARM64 da PR #3, e não apenas a pasta `macos-native` da branch antiga. É uma integração experimental real, com limitações documentadas; não estabelece compatibilidade geral com jogos. AOT #3891 é uma alternativa de pesquisa com evidência concentrada numa fixture qualificada, sem justificar substituição geral do runtime.

## Snapshots e alcance

| Caminho | Fonte fixada | Alcance observado |
|---|---|---|
| Checkout desta exploração | `c6dfbc0a3c869b36449ab3cd885a0fd3890d0b33`, `CMakeLists.txt:198` | Full build macOS rejeita arquitetura diferente de x86_64. Não basta mudar uma variável para obter HLE/runner ARM64. |
| `apple-native/macos-native` | [README em b391d4035052085b0cc0427773edf1fd960fb0c8](https://github.com/arvindfroi/AnyPS5/blob/b391d4035052085b0cc0427773edf1fd960fb0c8/macos-native/README.md) | Dois POCs: aritmética x86-64 e chamada a função ARM64. Não é loader/runner de jogos. |
| HLE ARM64, mugurc #3 | [PR](https://github.com/mugurc/AnyPS5/pull/3), head `a3070d3de65e731ab3735e2480b1053116fbdc1f` | Bibliotecas/testes nativos, ajustes ABI variádica, x87, assinatura e unwind. PR aberta, não integrada, na consulta. |
| Runner FEX, mugurc #4 | [CMake em 0220dfa194f3408b009e2df5a20c89cd074f26d2](https://github.com/arvindfroi/AnyPS5/blob/0220dfa194f3408b009e2df5a20c89cd074f26d2/core/fex/CMakeLists.txt), [PR](https://github.com/mugurc/AnyPS5/pull/4) | `aps5-fex`, dependente de #3. Carrega saída Linux do relinker, resolve módulos/imports, TLS e inicializadores; traduz CPU via FEXCore e chama HLE ARM64. PR aberta, não integrada. |
| AOT, boykopovar #3891 | [PR](https://github.com/boykopovar/AnyPS5/pull/3891), [head 67f94df827623df9210738a5ba3275b9a59f1c8b](https://github.com/yabanana/AnyPS5/tree/67f94df827623df9210738a5ba3275b9a59f1c8b) | PR fechada, `merged=false` na consulta. Fixture `gpu_cube` com 14 funções declaradas, contratos ligados a hashes e imports nativos; gráficos continuam Vulkan/MoltenVK. |

## FEX: maturidade e custos

O POC antigo fixa `willfaust/FEX` em `ac555dd81fa84f86d1927476e152451005e78f49`, identificado pelo autor como último commit MIT anterior às alterações GPL do fork. Seu `build.sh` baixa fontes/submódulos, compila FEXCore e executa os POCs: não deve ser tratado como comando apenas de preparação. O README descreve um pool de JIT de 1 GiB com vistas RW/RX via `mach_vm_remap`; reserva virtual não é uma medida de consumo residente ou desempenho.

A árvore da PR #4 contém `core/fex` e fixa `3rdparty/FEX` em `cad0c0969354e4f51462c5bf25f0f1acd5edce16`. Seu CMake usa `ExternalProject`, C++20, fontes ARM64 e bibliotecas estáticas FEXCore; não usa um FEX instalado no sistema. A PR descreve backports e patches macOS próprios. Isso implica manter um fork e revisar sua proveniência/licenças e sua evolução, além da ponte entre System V x86-64 e AAPCS64.

O autor relata 21 programas executados cinco vezes, incluindo TLS, módulos, threads, AVX2/FMA/BMI e callbacks, e 299 testes passando com 211 testes GPU pulados após rebase. Esses números são evidência relatada pelo autor, não validação nossa nem do backend KosmicKrisp. A PR #3 relata testes gráficos anteriores em MoltenVK com falhas e funcionalidades ausentes; eles também não qualificam KosmicKrisp.

As [limitações fixadas da PR #4](https://github.com/arvindfroi/AnyPS5/blob/0220dfa194f3408b009e2df5a20c89cd074f26d2/docs/dev/TechnicalDebt.md) incluem sinais guest não suportados, exceções cruzando frame host entre frames guest, callbacks ainda mediados por faults, `swprintf`, módulos dinâmicos restritos aos pré-carregados e mudanças de MXCSR/x87 em jumps/fibers que lançam erro. A ordenação vetorial permanece a padrão do FEX, diferente de x86; controles FTZ/DAZ separados foram verificados pelo autor apenas em M3 Pro. Esses itens podem afetar programas reais mesmo com fixtures verdes.

## KosmicKrisp: pré-requisitos e custo

A [documentação oficial Mesa](https://docs.mesa3d.org/drivers/kosmickrisp.html), consultada nesta data, descreve implementação Vulkan sobre Metal 4 em Apple Silicon e exige macOS 26+. A documentação é móvel, não uma versão fixada; os SHAs acima fixam as fontes AnyPS5. Ela também registra workarounds e opções experimentais, que não equivalem a suporte correto por padrão.

Para compilar o driver do zero, a documentação lista Xcode/CLT, Meson 1.9.1+, CMake, pkg-config, LLVM 20.1.8+, libclc, spirv-llvm-translator, SPIRV-Tools e pacotes Python. O SDK pode evitar essa compilação, mas sua biblioteca presente ainda exige validar loader, versão mínima de sistema, extensões/features e cada operação necessária pelo renderizador. API 1.4.0 no JSON não comprova todas as features opcionais.

O pipeline de shaders muda: a [apresentação técnica da LunarG, páginas 21–23](https://vulkan.org/user/pages/09.events/vulkanised-2026/1545-Richard-Wright-LunarG.pdf) descreve NIR → MSL; o [código Mesa fixado em `14a19dd6c167d0d5fd0126688318ed5461022588`](https://gitlab.freedesktop.org/mesa/mesa/-/blob/14a19dd6c167d0d5fd0126688318ed5461022588/src/kosmickrisp/kosmicomp.c), consultado pelo espelho de código, chama `spirv_to_nir` e `nir_to_msl`. Portanto, a conversão SPIR-V → MSL não passa pelo gerador MSL do SPIRV-Cross. LLVM/libclc e spirv-llvm-translator constam nas dependências de construção; isso não significa que os shaders Vulkan atravessem uma cadeia linear SPIR-V → LLVM → MSL. O compilador Metal ainda participa da geração final para a GPU.

Isso evita o caminho específico descrito em [KhronosGroup/SPIRV-Cross#2704](https://github.com/KhronosGroup/SPIRV-Cross/pull/2704): multiplicação precisa convertida em `fma(l, r, +0)`, perdendo o sinal de zero. É uma inferência sobre ausência desse gerador, não um teste de que KosmicKrisp preserva todos os resultados esperados. A mudança não garante tininess/denormais, Float64, atomics de 64 bits, ordenação de memória ou desempenho; continuam sujeitos às features anunciadas, ao hardware e à compilação Metal. A [documentação primária de workarounds](https://docs.mesa3d.org/drivers/kosmickrisp/workarounds.html) registra problemas próprios de NaN, coerência e compilação MSL. Não confundir essa PR com AnyPS5#2704, que trata de reservas de memória.

O custo inicial de FEX + KosmicKrisp reúne duas integrações independentes: CPU/HLE/ABI ARM64 e compatibilidade do backend Vulkan. Manter MoltenVK como referência permitiria atribuir falhas à camada correta. Não há nesta exploração medição de desempenho, consumo, estabilidade ou equivalência gráfica dessa combinação.

## AOT: limite da evidência

A descrição de #3891 relata ARM64 sem Rosetta/JIT/decoder runtime, 600 flips e oito comparações RGB exatas do cubo contra referência independente. Esses dados são do autor e de sua fixture, com shaders e contratos específicos; o próprio texto exclui compatibilidade geral PS5, recuperação geral de ABI, port completo ou demonstração de ganho de velocidade. Também relata dependência de #1835 e indisponibilidade de provedores matemáticos ligados a x87 em sua closure gráfica. Head fechado sem merge não deve ser apresentado como solução incorporada.

## Recomendação e próximos critérios

Preservar o runner x86-64 atual como baseline e registrar FEX #3/#4 como experimento ARM64 separado, com SHAs de todas as dependências fixados. Primeiro demonstrar build e fixtures CPU/HLE sem GPU; depois enumerar features em processo ARM64 e comparar testes gráficos mínimos entre MoltenVK e KosmicKrisp sem janela. A progressão deve exigir testes efetivamente executados nesta máquina, registrando skips e falhas. Só então avaliar uma carga mais ampla. AOT merece reprodução isolada da fixture como investigação de compilação, sem inferir que resolve o mesmo escopo do runner FEX.

Para decidir entre as rotas: priorizar FEX se a pergunta é executar saídas existentes do relinker com ponte HLE genérica; priorizar AOT se a pergunta é reproduzir e estudar a transformação fechada de um programa conhecido. Começar o experimento FEX pelas PRs #3/#4 evita reconstruir o runner a partir dos dois POCs. Não há base medida aqui para estimar dias de integração: faltam o primeiro build ARM64 e a comparação das dependências com este checkout. Um primeiro marco delimitado é configurar/compilar essas fontes fixadas e executar somente as fixtures CPU; ele permite estimar o trabalho restante com falhas concretas, sem prometer prazo ou compatibilidade de jogos.
