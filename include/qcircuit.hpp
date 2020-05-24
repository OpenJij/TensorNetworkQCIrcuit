//Copyright (c) 2020 Jij Inc.


#pragma once

#include "itensor/all.h"
#include <vector>
#include <utility>
#include <algorithm>
#include <functional>
#include <iostream>
#include <array>
#include "circuit_topology.hpp"

namespace qcircuit {
    using namespace itensor;

    /**
     * @brief Class to store and modify wave function.
     */
    class QCircuit {
    private:
        const CircuitTopology topology; //!< @brief Circuit topology.

        std::vector<Index> a;   //!< @brief Link (bond) indices.
        std::vector<Index> s;   //!< @brief Physical (on-site) indices.
        std::vector<ITensor> M; //!< @brief Tensor entities.
        ITensor Psi;            //!< @brief TPS wave function.

        std::pair<std::size_t, std::size_t> cursor; //!< @brief Position of cursor, which must be laid across two neighboring sites.

    public:
        /** @brief Constructor to initialize TPS wave function.
         *
         * Cursor position will be set at (0, 1).
         *
         * @param topology Circuit Topology.
         * @param init_qbits Initial qubit states for each site.
         * @param physical_indices Physical (on-site) indices.
         * If not specified, the indices will be initialized with new IDs.
         * This argument is mainly used to share physical indices among
         * some "replica" wave functions in the same circuit.
         **/
        QCircuit(const CircuitTopology& topology,
                 const std::vector<std::pair<std::complex<double>, std::complex<double>>>& init_qbits,
                 const std::vector<Index>& physical_indices = std::vector<Index>()) :
            a(), s(physical_indices), topology(topology) {

            /* Initialize link indices */
            a.reserve(topology.numberOfLinks());
            for(size_t i = 0; i < topology.numberOfLinks();i++) {
                a.emplace_back(1, "LinkInd");
            }

            /* Initialize physical indices */
            if(s.empty()) {
                s.reserve(this->size());
                for(size_t i = 0;i < this->size();i++) {
                    s.emplace_back(2, "SiteInd");
                }
            }

            /* Initialize tensor entities */
            M.reserve(this->size());
            for(size_t i = 0;i < this->size();i++) {
                auto neighbors = topology.neighborsOf(i);

                switch(neighbors.size()) {
                case 1:
                    M.emplace_back(s[i],   a[neighbors[0].link]);
                    M[i].set(      s[i]=1, a[neighbors[0].link]=1, init_qbits[i].first);
                    M[i].set(      s[i]=2, a[neighbors[0].link]=1, init_qbits[i].second);
                    break;
                case 2:
                    M.emplace_back(s[i],   a[neighbors[0].link],   a[neighbors[1].link]);
                    M[i].set(      s[i]=1, a[neighbors[0].link]=1, a[neighbors[1].link]=1, init_qbits[i].first);
                    M[i].set(      s[i]=2, a[neighbors[0].link]=1, a[neighbors[1].link]=1, init_qbits[i].second);
                    break;
                case 3:
                    M.emplace_back(s[i],   a[neighbors[0].link]  , a[neighbors[1].link]  , a[neighbors[2].link]);
                    M[i].set(      s[i]=1, a[neighbors[0].link]=1, a[neighbors[1].link]=1, a[neighbors[2].link]=1, init_qbits[i].first);
                    M[i].set(      s[i]=2, a[neighbors[0].link]=1, a[neighbors[1].link]=1, a[neighbors[2].link]=1, init_qbits[i].second);
                    break;
                default:
                    assert(false);
                }
            }

            cursor.first = 0;
            cursor.second = 1;

            Psi = M[cursor.first]*M[cursor.second];
        }

        /** @brief returns number of qubits */
        size_t size() const {
            return this->topology.numberOfBits();
        }

        /** @brief decompose and truncate the system wave-function at cursor position.  */
        void decomposePsi(Args args = Args::global()){
            ITensor U,S,V;

            // fetch nodelist in index "first"
            auto list = topology.neighborsOf(cursor.first);

            /* get link index corresponding to cursor position
             * (this can be combined with the std::remove_if below)
             */
            auto second_itr = std::find_if(list.begin(), list.end(),
                                              [&](auto x) {return x.site == cursor.second;});
            auto link_ind = (*second_itr).link;

            // remove the element which connects to node second
            auto pend = std::remove_if(list.begin(), list.end(), [&](const auto& t){return t.site == cursor.second;});
            std::vector<CircuitTopology::Neighbor> newlist;
            for(auto it = list.begin(); it != pend; ++it){
                newlist.push_back(*it);
            }
            switch(newlist.size()) {
            case 0:
                U = ITensor(s[cursor.first]);
                break;
            case 1:
                U = ITensor(s[cursor.first], a[newlist[0].link]);
                break;
            case 2:
                U = ITensor(s[cursor.first], a[newlist[0].link], a[newlist[1].link]);
                break;
            default:
                assert(false);
            }

            Spectrum spec = svd(Psi, U, S, V, args);
            a[link_ind] = commonIndex(U,S);
            S /= norm(S); //normalize
            M[cursor.first] = U;
            M[cursor.second] = S*V;
        }

        /** @brief shifts cursor position to specified neighboring site `ind`   */
        Spectrum shift_to(size_t ind, const Args& args = Args::global()) {
            assert(ind != cursor.first);
            assert(ind != cursor.second);

            //search ind from list
            int flag = 0; //0 not found 1 connected with cursor.first  2 connected with cursor.second

            //first
            for(auto&& i : topology.neighborsOf(cursor.first)) {
                if(i.site == ind){
                    flag = 1;
                    break;
                }
            }
            //second
            for(auto&& i : topology.neighborsOf(cursor.second)) {
                if(i.site == ind){
                    flag = 2;
                    break;
                }
            }

            Spectrum spec;

            if(flag == 1) {
                ITensor U,S,V;

                //fetch nodelist in index "second"
                auto list = this->topology.neighborsOf(cursor.second);
                //get link index
                size_t link_ind = 0;
                for(auto&& i : list){
                    if(i.site == cursor.first){
                        link_ind = i.link;
                    }
                }
                //remove the element which connects to node first
                auto pend = std::remove_if(list.begin(), list.end(), [&](const auto& t){return t.site == cursor.first;});
                std::vector<CircuitTopology::Neighbor> newlist;
                for(auto it = list.begin(); it != pend; ++it){
                    newlist.push_back(*it);
                }
                switch(newlist.size()){
                case 0:
                    V = ITensor(s[cursor.second]);
                    break;
                case 1:
                    V = ITensor(s[cursor.second], a[newlist[0].link]);
                    break;
                case 2:
                    V = ITensor(s[cursor.second], a[newlist[0].link], a[newlist[1].link]);
                    break;
                default:
                    assert(false);
                }

                spec = svd(Psi, U, S, V, args);

                a[link_ind] = commonIndex(S,V);
                S /= norm(S); //normalize
                M[cursor.second] = V;
                Psi = M[ind]*U*S;

                cursor.second = cursor.first;
                cursor.first = ind;
            } else if(flag == 2) {
                ITensor U,S,V;

                //fetch nodelist in index "first"
                auto list = topology.neighborsOf(cursor.first);
                //get link index
                size_t link_ind = 0;
                for(auto&& i : list) {
                    if(i.site == cursor.second){
                        link_ind = i.link;
                    }
                }
                //remove the element which connects to node second
                auto pend = std::remove_if(list.begin(), list.end(), [&](const auto& t){return t.site == cursor.second;});
                std::vector<CircuitTopology::Neighbor> newlist;
                for(auto it = list.begin(); it != pend; ++it){
                    newlist.push_back(*it);
                }
                switch(newlist.size()){
                case 0:
                    U = ITensor(s[cursor.first]);
                    break;
                case 1:
                    U = ITensor(s[cursor.first], a[newlist[0].link]);
                    break;
                case 2:
                    U = ITensor(s[cursor.first], a[newlist[0].link], a[newlist[1].link]);
                    break;
                default:
                    assert(false);
                }

                spec = svd(Psi, U, S, V, args);

                a[link_ind] = commonIndex(U,S);
                S /= norm(S); //normalize
                M[cursor.first] = U;
                Psi = S*V*M[ind];

                cursor.first = cursor.second;
                cursor.second = ind;
            } else {
                assert(false && "cannot move to this direction");
            }

            return spec;
        }

        /** @brief apply `op` at cursor position.  */
        void apply(const ITensor& op) {
            assert(op.inds().size() == 4);
            for(auto&& elem : op.inds()){
                //check if
                assert(elem == s[cursor.first] || elem == s[cursor.second] || elem == prime(s[cursor.first]) || elem == prime(s[cursor.second]));
            }

            this->Psi = op * prime(Psi, s[cursor.first], s[cursor.second]);
        }

        void normalize() {
            Psi /= norm(Psi);
        }

        void primeAll() {
            for(auto& elem : this->s){
                elem = prime(elem);
            }

            for(auto& elem : this->a){
                elem = prime(elem);
            }

            for(auto& elem : this->M){
                elem = prime(elem);
            }

            Psi = prime(Psi);
        }

        const ITensor& Mref(size_t i) const {
            assert(0 <= i && i < this->size());
            return this->M[i];
        }

        const std::vector<ITensor>& Mref() const {
            return this->M;
        }

        const ITensor& Psiref() const {
            return this->Psi;
        }

        const Index& site(size_t i) const {
            assert(0 <= i && i < this->size());
            return this->s[i];
        }

        const std::vector<Index>& site() const {
            return this->s;
        }

        const std::pair<size_t, size_t>& getCursor() const {
            return this->cursor;
        }

        void PrintMat() const {
            for(auto&& elem : M){
                PrintData(elem);
            }
            std::cout << "-----------" << std::endl;
            PrintData(Psi);
        }

        void PrintCursor() const {
            std::cout << "(" << cursor.first << "," << cursor.second << ")" << std::endl;
        }
    };

	//ITensor Hamiltonian
	inline const ITensor Id(const Index& s) {
		ITensor ret(s, prime(s));
		ret.set(s(1), prime(s)(1), 1);
		ret.set(s(2), prime(s)(2), 1);
		return ret;
	}

    //pauli x
	inline const ITensor X(const Index& s) {
		ITensor ret(s, prime(s));
		ret.set(s(1), prime(s)(2), 1);
		ret.set(s(2), prime(s)(1), 1);
		return ret;
	}

    //pauli y
	inline const ITensor Y(const Index& s) {
		ITensor ret(s, prime(s));
		ret.set(s(1), prime(s)(2), -1_i);
		ret.set(s(2), prime(s)(1), 1_i);
		return ret;
	}

    //pauli z
	inline const ITensor Z(const Index& s) {
		ITensor ret(s, prime(s));
		ret.set(s(1), prime(s)(1), 1);
		ret.set(s(2), prime(s)(2), -1);
		return ret;
	}

    //|0><0|
	inline const ITensor proj_0(const Index& s) {
		ITensor ret(s, prime(s));
		ret.set(s(1), prime(s)(1), 1);
		return ret;
	}

    //|1><1|
	inline const ITensor proj_1(const Index& s) {
		ITensor ret(s, prime(s));
		ret.set(s(2), prime(s)(2), 1);
		return ret;
	}

    //|1><0|
	inline const ITensor proj_0_to_1(const Index& s) {
		ITensor ret(s, prime(s));
		ret.set(s(2), prime(s)(1), 1);
		return ret;
	}

    //|0><1|
	inline const ITensor proj_1_to_0(const Index& s) {
		ITensor ret(s, prime(s));
		ret.set(s(1), prime(s)(2), 1);
		return ret;
	}

    //Hadamard
	inline const ITensor H(const Index& s) {
		return (1.0/sqrt(2.0))*(proj_0(s) + proj_0_to_1(s)) + (1.0/sqrt(2.0))*(proj_1(s) - proj_1_to_0(s));
	}

    //CNOT
	inline const ITensor CNOT(const Index& s1, const Index& s2) {
		return proj_0(s1)*Id(s2) + proj_1(s1)*X(s2);
	}

    //Control-Y
	inline const ITensor CY(const Index& s1, const Index& s2) {
		return proj_0(s1)*Id(s2) + proj_1(s1)*Y(s2);
	}

    //Control-Z
	inline const ITensor CZ(const Index& s1, const Index& s2) {
		return proj_0(s1)*Id(s2) + proj_1(s1)*Z(s2);
	}




	Cplx overlap(QCircuit circuit1, const std::vector<ITensor>& op, QCircuit circuit2,
                 const Args& args = Args::global()) {
		assert(op.size() == circuit1.size() && op.size() == circuit2.size());

		circuit1.decomposePsi(args);
		circuit2.decomposePsi(args);

		circuit2.primeAll();

		ITensor ret_t = dag(circuit1.Mref(0))*op[0]*circuit2.Mref(0);
		for(size_t i = 1; i < circuit1.size(); i++) {
			//reduction
			ret_t = dag(circuit1.Mref(i))*op[i]*ret_t*circuit2.Mref(i);
		}

		return ret_t.cplx();
	}

} // namespace qcircuit

