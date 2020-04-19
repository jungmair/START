#pragma once
//base class for factories producing some kind of rewiring provider
class rewiring_provider_creator {
public:
    virtual std::shared_ptr<rewiring_provider> createProvider(size_t len)=0;
    virtual ~rewiring_provider_creator() = default;

};